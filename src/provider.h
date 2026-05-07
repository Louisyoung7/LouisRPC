#pragma once

#include <functional>
#include <unordered_map>

#include "net/InetAddress.h"
#include "net/TcpServer.h"

class Provider {
   public:
    using RpcHandler = std::function<std::string(const std::string&)>;
    Provider(const std::string& ip, int port);
    ~Provider() = default;

    // 启动服务端
    void start();

    // 注册服务
    template <typename Req, typename Resp, typename Func>
    void registerService(const std::string& serviceName, const std::string& methodName, Func&& handler);

   private:
    net::reactor::EventLoop eventLoop_;
    net::InetAddress listenAddr_;
    net::TcpServer server_;
    std::unordered_map<std::string, std::unordered_map<std::string, RpcHandler>> services_;

    void onConnection(const net::TcpServer::TcpConnectionPtr& conn);
    void onMessage(const net::TcpServer::TcpConnectionPtr& conn, base::Buffer& buffer);
};