#include "RpcClient.h"

#include <future>

#include "codec/RpcCodec.h"
#include "log/Logger.h"
#include "net/TcpConnection.h"

RpcClient::RpcClient(net::reactor::EventLoop* loop, const net::InetAddress& serverAddr, const std::string& name)
    : client_(loop, serverAddr, name) {
    client_.setConnectionCallback([this](const net::TcpConnectionPtr& conn) { onConnection(conn); });
    client_.setMessageCallback([this](const net::TcpConnectionPtr& conn, base::Buffer& buf) { onMessage(conn, buf); });
}

RpcClient::~RpcClient() {
    // 所有待处理调用以 kConnectionLost 错误失败
    // 避免同步调用线程阻塞在 future.get() 上
    failAllPending(CallError::kConnectionLost);
}

void RpcClient::callAsync(const std::string& serviceName, const std::string& methodName,
                          const std::vector<char>& params, int timeoutMs, ResultCallback callback) {
    if (!callback) {
        error("[RpcClient] callAsync callback is null.");
        return;
    }

    const uint32_t msgId = nextMsgId_.fetch_add(1);

    // 1.组装并编码请求
    RpcMessage request;
    request.msgType = MsgType::Request;
    request.msgId = msgId;
    request.payload = RequestPayload{serviceName, methodName, params};

    base::Buffer output;
    if (RpcCodec::encode(request, output) < 0) {
        error("[RpcClient] encode request failed, msgId: {}", msgId);
        // 事件循环线程中调用 runInLoop 会同步执行，回调可能在返回前完成
        client_.getLoop()->runInLoop([cb = std::move(callback)]() { cb(CallResult{CallError::kSendFailed}); });
        return;
    }
    std::string encoded = output.retrieveAllAsString();

    // 2.登记在途调用
    auto pending = std::make_shared<PendingCall>();

    // 注册定时器
    pending->timerId = client_.getLoop()->runAfter(std::chrono::milliseconds(timeoutMs < 0 ? 0 : timeoutMs),
                                                   [this, msgId]() { onTimeout(msgId); });

    // 待处理调用入表
    // 待处理调用入表
    net::TcpConnectionPtr conn;
    ResultCallback failCb;  // 连接未建立时的回调，锁外处理
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!conn_ || !conn_->connected()) {
            failCb = std::move(callback);
            client_.getLoop()->cancel(pending->timerId);
            if (!conn_) {
                info("It is no conn_.");
            } else if (!conn_->connected()) {
                info("It is not connected.");
            } else {
                info("Everything is not ok.");
            }
            return;
        } else {
            pending->callback = std::move(callback);
            conn = conn_;
            pendingCalls_.emplace(msgId, std::move(pending));
        }
    }
    if (failCb) {
        client_.getLoop()->runInLoop([cb = std::move(failCb)]() { cb(CallResult{CallError::kNotConnected}); });
        return;
    }

    // 3. 切回事件循环线程，发送请求
    auto data = std::make_shared<std::string>(encoded);
    client_.getLoop()->runInLoop([this, conn, data, msgId]() {
        if (!conn->connected()) {
            completeCall(msgId, CallResult{CallError::kSendFailed});
            return;
        }
        conn->send(*data);
    });
}

RpcClient::CallResult RpcClient::call(const std::string& serviceName, const std::string& methodName,
                                      const std::vector<char>& request, int timeoutMs) {
    // 在事件循环线程调用会死锁：响应需要 loop 线程处理，而此时 loop 线程阻塞在future.get() 上
    if (client_.getLoop()->isInLoopThread()) {
        error("[RpcClient] call() must be not called in loop thread.");
        return CallResult{CallError::kInvalidUsage};
    }

    auto promise = std::make_shared<std::promise<CallResult>>();
    auto future = promise->get_future();
    callAsync(serviceName, methodName, request, timeoutMs,
              [promise](CallResult result) { promise->set_value(std::move(result)); });
    return future.get();  // 由响应到达/超时/断连/析构/错误等唤醒
}

void RpcClient::onConnection(const net::TcpConnectionPtr& conn) {
    if (conn->connected()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            conn_ = conn;
        }
        info("[RpcClient] connection {} established.", conn->name());

        // 通知上层连接建立
        if (connectionCb_) {
            connectionCb_(true);
        }
    } else {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            conn_.reset();
        }
        // 连接断开，失败所有待处理调用
        failAllPending(CallError::kConnectionLost);
        info("[RpcClient] connection {} closed.", conn->name());

        // 通知上层连接断开
        if (connectionCb_) {
            connectionCb_(false);
        }
    }
}

void RpcClient::onMessage(const net::TcpConnectionPtr& conn, base::Buffer& buffer) {
    while (true) {
        RpcMessage msg;
        const ssize_t n = RpcCodec::decode(buffer, msg);

        if (n > 0) {
            if (!msg.isRequest()) {
                // 响应正常到达，提取响应负载，完成此次调用
                const ResponsePayload& resp = msg.getResponsePayload();
                completeCall(msg.msgId, CallResult{CallError::kNone, resp.statusCode, resp.result});
            } else {
                warn("[RpcClient] received a request message, protocol violation, closing.");
                conn->shutdown();
                return;
            }
        } else if (n == 0) {
            // 半包，直接返回，等待下一次读取
            break;
        } else {
            warn("[RpcClient] protocol error, closing.");
            conn->shutdown();
            return;
        }
    }
}

void RpcClient::onTimeout(uint32_t msgId) {
    warn("[RpcClient] timeout msgId: {}", msgId);
    completeCall(msgId, CallResult{CallError::kTimeout});
}

void RpcClient::completeCall(uint32_t msgId, CallResult result) {
    // 从表中摘取是唯一的所有权转移，天然互斥 响应/超时/断连/失败 等之间的竞争（均在事件循环线程串行执行）
    std::shared_ptr<PendingCall> pending;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pendingCalls_.find(msgId);
        if (it == pendingCalls_.end()) return;
        pending = std::move(it->second);
        pendingCalls_.erase(it);
    }

    client_.getLoop()->cancel(pending->timerId);
    pending->callback(std::move(result));
}

void RpcClient::failAllPending(CallError error) {
    std::unordered_map<uint32_t, std::shared_ptr<PendingCall>> doomed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        doomed.swap(pendingCalls_);
    }

    // 调用点均在事件循环线程（断连/析构），回调直接执行
    for (auto& [_, pending] : doomed) {
        client_.getLoop()->cancel(pending->timerId);
        pending->callback(CallResult{error});
    }
}
