#include "RpcChannel.h"

#include <vector>

#include "codec/RpcCodec.h"
#include "proto/header.pb.h"

using namespace net;
using namespace net::reactor;

template <typename Req, typename Resp>
void RpcChannel::call(const std::string& serviceName, const std::string& methodName, const Req& request, Resp& response,
                      RpcCallback callback) {
    // 1.序列化请求参数
    size_t reqSize = request.ByteSizeLong();
    std::vector<char> reqBuf(reqSize);
    request.SerializeToArray(reqBuf.data(), reqSize);

    // 2.生成唯一请求ID
    uint64_t requestId = nextRequestId_.fetch_add(1, std::memory_order_relaxed);

    // 3.构建请求header
    louis_rpc::Header header;
    header.set_request_id(requestId);
    header.set_service_name(serviceName);
    header.set_method_name(methodName);
    header.set_param_len(reqSize);

    // 4.存储待处理请求
    PendingRequest pending;
    pending.serviceName = serviceName;
    pending.methodName = methodName;
    pending.requestData = std::move(reqBuf);
    pending.callback = std::move(callback);
    pendingRequests_[requestId] = std::move(pending);

    // 5.编码完整报文
    std::vector<char> reqData = RpcCodec::encode(header, pendingRequests_[requestId].requestData.data(), reqSize);

    // 6.发送请求报文
    conn_->send(reqData.data(), reqData.size());
}

void RpcChannel::onMessage(const net::TcpConnectionPtr& conn, base::Buffer& buf) {
    while (buf.readableBytes() >= kLengthFieldSize) {
        auto packet = RpcCodec::decode(buf);
        if (!packet) { break; }

        // 1.获取请求ID
        uint64_t requestId = packet.value().header.request_id();

        // 2.根据请求ID查找待处理请求
        auto pending = pendingRequests_.find(requestId);
        if (pending == pendingRequests_.end()) { continue; }

        // 3.调用回调函数
        pending->second.callback(packet.value().paramData);

        // 4.删除待处理请求
        pendingRequests_.erase(requestId);
    }
}