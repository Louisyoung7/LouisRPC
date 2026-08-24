#include "RpcServer.h"

#include "codec/RpcCodec.h"
#include "service/ServiceRegistry.h"

namespace {
std::vector<char> toBytes(const std::string& str) { return {str.begin(), str.end()}; }
}  // namespace

RpcServer::RpcServer(net::reactor::EventLoop* loop, const net::InetAddress& listenAddr, size_t threadNum,
                     const std::string& name)
    : server_(loop, listenAddr, name), threadPool_(std::make_unique<base::ThreadPool>(threadNum == 0 ? 1 : threadNum)) {
    server_.setConnectionCallback([this](const net::TcpConnectionPtr& conn) { onConnection(conn); });
    server_.setMessageCallback([this](const net::TcpConnectionPtr& conn, base::Buffer& buf) { onMessage(conn, buf); });
}

RpcServer::~RpcServer() {
    // 标记停止，阻止向线程池提交新任务
    stopping_.store(true);
    // 成员按照声明逆序析构，先析构线程池，再析构服务注册表
    // 确保线程池析构，join正在执行的任务时，注册表有效
}

void RpcServer::onConnection(const net::TcpConnectionPtr& conn) {
    if (conn->connected()) {
        info("[RpcServer] connection {} established.", conn->name());
    } else {
        info("[RpcServer] connection {} closed.", conn->name());
    }
}

void RpcServer::onMessage(const net::TcpConnectionPtr& conn, base::Buffer& buf) {
    // 从缓冲区读取消息，循环解码，避免粘包半包问题
    while (true) {
        RpcMessage msg;
        const ssize_t n = RpcCodec::decode(buf, msg);

        if (n > 0) {
            if (msg.isRequest()) {
                handleRequest(conn, std::move(msg));
            } else {
                // 服务端只处理请求消息，不处理响应消息
                warn("[RpcServer] connection {} received non-request message, protocol violation.", conn->name());
                conn->shutdown();
                return;
            }
        } else if (n == 0) {
            // 半包，直接返回，等待下一次读取
            break;
        } else {
            warn("[RpcServer] connection {} protocol error, closing.", conn->name());
            conn->shutdown();
            return;
        }
    }
}

void RpcServer::handleRequest(const net::TcpConnectionPtr& conn, RpcMessage request) {
    if (stopping_.load()) return;

    const uint32_t msgId = request.msgId;
    RequestPayload req = std::move(std::get<RequestPayload>(request.payload));

    try {
        threadPool_->submit([this, conn, msgId, req = std::move(req)]() mutable {
            // 任务分发
            DispatchResult dr = registry_.dispatch(req.serviceName, req.methodName, req.params);

            // 构建响应消息结构体
            RpcMessage resp;
            resp.msgType = MsgType::Response;
            resp.msgId = msgId;
            resp.payload = ResponsePayload{dr.status, std::move(dr.result)};

            base::Buffer output;
            // 尝试编码响应消息，若失败则构造 fallback 响应消息
            if (RpcCodec::encode(std::move(resp), output) < 0) {
                error("[RpcServer] encode response failed for msgId {}", msgId);
                RpcMessage fallback;
                fallback.msgType = MsgType::Response;
                fallback.msgId = msgId;
                fallback.payload = ResponsePayload{RpcStatus::INTERNAL_ERROR, toBytes("encode response failed")};
                // 清空Buffer后，编码 fallback 响应消息
                output.consumeAll();
                RpcCodec::encode(fallback, output);
            }

            // 获取编码后的响应消息
            std::string encoded = output.retrieveAllAsString();

            // 切回事件循环线程同步发送
            // 由 lambda 捕获 shared_ptr<string> 确保数据生命周期
            // 通过 runInLoop 将 send() 推送到事件循环线程
            // 规避 send() 跨线程分支裸指针捕获导致的悬垂指针问题
            conn->getLoop()->runInLoop([conn, data = std::make_shared<std::string>(std::move(encoded))]() {
                if (conn->connected()) conn->send(*data);
            });
        });
    } catch (const std::exception& e) {
        // 线程池析构期间提交任务
        error("[RpcServer] submit task failed: {}", e.what());
    }
}
