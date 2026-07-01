#pragma once

#include <unistd.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "net/TcpConnection.h"

class RpcChannel {
    using RpcCallback = std::function<void(const std::vector<char>&)>;

   public:
    RpcChannel(net::TcpConnectionPtr conn) : conn_(conn) {}

    template <typename Req, typename Resp>
    void call(const std::string& serviceName, const std::string& methodName, const Req& request, Resp& response,
              RpcCallback callback);

   private:
    struct PendingRequest {
        std::string serviceName;
        std::string methodName;
        std::vector<char> requestData;  // 请求数据
        RpcCallback callback;
    };

    net::TcpConnectionPtr conn_;
    std::unordered_map<uint64_t, PendingRequest> pendingRequests_;
    std::atomic<uint64_t> nextRequestId_{0};

    void onMessage(const net::TcpConnectionPtr& conn, base::Buffer& buf);
};