#pragma once

#include <functional>
#include <unordered_map>

#include "net/InetAddress.h"
#include "net/TcpServer.h"

class Provider {
   public:
    Provider(const std::string& ip, int port);
    ~Provider() = default;

    // 启动服务端
    void start();

    // 注册服务
    void registerService(const std::string& serviceName, std::function<int(int, int)> service);

   private:
    net::reactor::EventLoop eventLoop_;
    net::InetAddress listenAddr_;
    net::TcpServer server_;
    std::unordered_map<std::string, std::function<int(int, int)>> services_;

    void onConnection(const net::TcpServer::TcpConnectionPtr& conn);
    void onMessage(const net::TcpServer::TcpConnectionPtr& conn, base::Buffer& buffer);
};