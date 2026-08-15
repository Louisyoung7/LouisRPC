#pragma once

#include "base/ThreadPool.h"
#include "codec/Message.h"
#include "log/Logger.h"
#include "net/InetAddress.h"
#include "net/TcpServer.h"
#include "service/ServiceRegistry.h"

namespace net::reactor {
class EventLoop;
}

class RpcServer {
   public:
    RpcServer(net::reactor::EventLoop* loop, const net::InetAddress& listenAddr, size_t threadNum = 4,
              const std::string& name = "RpcServer");
    ~RpcServer();

    // 注册服务，空指针/空名/重名服务返回false
    // 必须在start前完成所有服务注册，暂不支持动态注册服务
    bool registerService(std::unique_ptr<Service> service) { return registry_.registerService(std::move(service)); }

    // 启动服务器
    void start() {
        server_.start();
        info("[RpcServer] listening on {}.", server_.listenAddr().toIpPort());
    }

   private:
    // 连接建立/断开
    void onConnection(const net::TcpConnectionPtr& conn);
    // 收包，循环 encode 解码
    void onMessage(const net::TcpConnectionPtr& conn, base::Buffer& buf);

    void handleRequest(const net::TcpConnectionPtr& conn, RpcMessage request);

    net::TcpServer server_;
    ServiceRegistry registry_;  // 服务注册表（运行时只读，可被多个线程安全访问）
    std::unique_ptr<base::ThreadPool> threadPool_;  // 业务线程池
    std::atomic_bool stopping_{false};              // 析构保护：阻止向正在销毁的线程池提交任务
};