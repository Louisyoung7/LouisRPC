#pragma once

#include "codec/Protocol.h"
#include "net/TcpClient.h"

namespace net::reactor {
class EventLoop;
}

class RpcClient {
   public:
    // 客户端框架层错误码
    enum class CallError : uint8_t {
        kNone = 0,        // 正常收到响应
        kTimeout,         // 等待超时
        kNotConnected,    // 发起调用时未连接到服务器
        kConnectionLost,  // 等待期间连接丢失或断开
        kSendFailed,      // 编码或发送请求失败
        kInvalidUsage,    // 在事件循环线程调用 call()
    };

    // RPC 调用结果结构体
    struct CallResult {
        CallError error = CallError::kNone;  // 客户端框架层错误码
        RpcStatus status = RpcStatus::OK;    // RPC 状态码
        std::vector<char> result;            // 服务端返回结果/错误信息

        bool ok() const { return error == CallError::kNone; }
    };

    // callAsync 的结果回调函数类型
    using ResultCallback = std::function<void(CallResult)>;
    // 连接建立/断开回调函数类型
    using ConnectionCallback = std::function<void(bool connected)>;

    RpcClient(net::reactor::EventLoop* loop, const net::InetAddress& serverAddr, const std::string& name = "RpcClient");
    ~RpcClient();

    // 发起连接
    void connect() { client_.connect(); }
    // 主动断开连接（在途调用将以 kConnectionLost 错误失败）
    void disconnect() { client_.disconnect(); }

    // 异步回调调用：立即返回，完成（响应/超时/失败）时在事件循环线程调用回调
    // callback 不可为空，timeoutMs <= 0 表示立即超时
    void callAsync(const std::string& serviceName, const std::string& methodName, const std::vector<char>& params,
                   int timeoutMs, ResultCallback callback);

    // 同步阻塞调用：基于 callAsync + promise/future 实现
    // timeoutMs <= 0 表示立即超时
    CallResult call(const std::string& serviceName, const std::string& methodName, const std::vector<char>& request,
                    int timeoutMs = 3000);

    // 设置连接建立/断开回调函数
    void setConnectionCallback(ConnectionCallback cb) { connectionCb_ = std::move(cb); }

   private:
    // 待处理调用结构体
    struct PendingCall {
        ResultCallback callback;  // 调用结果回调函数
        timer::TimerId timerId;   // 超时定时器句柄
    };

    // 连接建立/断开
    void onConnection(const net::TcpConnectionPtr& conn);
    // 收包：响应到达时循环 decode
    void onMessage(const net::TcpConnectionPtr& conn, base::Buffer& buffer);
    // 超时回调
    void onTimeout(uint32_t msgId);

    // 完成一次调用：通过 msgId 摘表，通过 result 调用回调
    void completeCall(uint32_t msgId, CallResult result);

    // 所有待处理调用通过 error 指定错误后失败
    void failAllPending(CallError error);

    net::TcpClient client_;
    net::TcpConnectionPtr conn_;
    std::mutex mutex_;                                                         // 保护 conn_ 和 pendingCalls_
    std::unordered_map<uint32_t, std::shared_ptr<PendingCall>> pendingCalls_;  // 待处理调用表：消息ID -> 待处理调用
    std::atomic<uint32_t> nextMsgId_{1};                                       // 下一个消息ID
    ConnectionCallback connectionCb_;                                          // 连接建立/断开回调函数
};
