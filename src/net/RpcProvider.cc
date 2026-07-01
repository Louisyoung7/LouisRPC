#include "RpcProvider.h"

#include <arpa/inet.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

#include "codec/RpcCodec.h"
#include "proto/header.pb.h"

using namespace std;

RpcProvider::RpcProvider(const std::string& ip, int port) : listenAddr_(ip, port), server_(&eventLoop_, listenAddr_) {
    server_.setConnectionCallback([this](const net::TcpServer::TcpConnectionPtr& conn) { onConnection(conn); });
    server_.setMessageCallback(
        [this](const net::TcpServer::TcpConnectionPtr& conn, base::Buffer& buffer) { onMessage(conn, buffer); });
}

void RpcProvider::start() {
    server_.start();
    eventLoop_.loop();
}

// 注册服务
// 注册某服务的方法的处理函数的同时，也会同时处理Message的序列化和反序列化
template <typename Req, typename Resp, typename Func>
void RpcProvider::registerService(const std::string& serviceName, const std::string& methodName, Func&& handler) {
    services_[serviceName][methodName] = [handler](const std::vector<char>& request) -> std::vector<char> {
        // 反序列化请求
        Req req;
        req.ParseFromArray(request.data(), request.size());
        // 调用方法
        Resp resp = handler(req);
        // 返回序列化响应
        std::vector<char> respBuf(resp.ByteSizeLong());
        resp.SerializeToArray(respBuf.data(), respBuf.size());
        return respBuf;
    };
}

void RpcProvider::onConnection(const net::TcpServer::TcpConnectionPtr& conn) {
    if (conn->connected()) {
        cout << "Client connected" << endl;
    } else {
        cout << "Client disconnected" << endl;
    }
}

void RpcProvider::onMessage(const net::TcpServer::TcpConnectionPtr& conn, base::Buffer& buffer) {
    std::optional<RpcCodec::DecodedPacket> decodedPacket = RpcCodec::decode(buffer);
    if (!decodedPacket.has_value()) { return; }

    const auto& packet = decodedPacket.value();

    // 获取服务名和方法名
    const std::string& serviceName = packet.header.service_name();
    const std::string& methodName = packet.header.method_name();

    // 调用方法
    std::vector<char> respBuf = services_[serviceName][methodName](std::move(packet.paramData));

    // 发送响应
    conn->send(respBuf.data(), respBuf.size());
}
