#include "provider.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

#include "../example/calculator.pb.h"

using namespace std;

Provider::Provider(const std::string& ip, int port) : listenAddr_(ip, port), server_(&eventLoop_, listenAddr_) {
    server_.setConnectionCallback([this](const net::TcpServer::TcpConnectionPtr& conn) { onConnection(conn); });
    server_.setMessageCallback(
        [this](const net::TcpServer::TcpConnectionPtr& conn, base::Buffer& buffer) { onMessage(conn, buffer); });
}

void Provider::start() {
    server_.start();
    eventLoop_.loop();
}

// 注册服务
void Provider::registerService(const std::string& serviceName, std::function<int(int, int)> service) {
    services_[serviceName] = service;
}

void Provider::onConnection(const net::TcpServer::TcpConnectionPtr& conn) {
    if (conn->connected()) {
        cout << "Client connected" << endl;
    } else {
        cout << "Client disconnected" << endl;
    }
}

void Provider::onMessage(const net::TcpServer::TcpConnectionPtr& conn, base::Buffer& buffer) {
    if (buffer.readableBytes() < 4) { return; }
    cout << "Receive message" << endl;
    // 读取总长度
    int totalLen = 0;
    std::memcpy(&totalLen, buffer.peek(), sizeof(totalLen));

    // 检查总长度是否有效
    if (totalLen > static_cast<int>(buffer.readableBytes()) - static_cast<int>(sizeof(totalLen))) { return; }

    cout << "Total length: " << totalLen << endl;

    // 读取数据
    vector<char> data(totalLen);
    memcpy(data.data(), buffer.peek() + sizeof(totalLen), totalLen);
    buffer.consume(totalLen + sizeof(totalLen));

    // 解析请求
    calculator::AddRequest request;
    request.ParseFromArray(data.data(), totalLen);

    // 调用服务
    auto service = services_[request.name()];
    assert(service != nullptr);
    int result = service(request.a(), request.b());

    // 发送结果
    vector<char> response(sizeof(result));
    memcpy(response.data(), &result, sizeof(result));
    conn->send(response.data(), sizeof(result));
    cout << "Send response done" << endl;
}
