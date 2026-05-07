#include "provider.h"

#include <arpa/inet.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

#include "header.pb.h"

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
template <typename Req, typename Resp, typename Func>
void Provider::registerService(const std::string& serviceName, const std::string& methodName, Func&& handler) {
    services_[serviceName][methodName] = [handler](const std::string& args) -> std::string {
        Req request;
        if (!request.ParseFromString(args)) { return ""; }
        Resp response = handler(request);
        return response.SerializeAsString();
    };
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

    // 检查数据是否完整
    if (totalLen > static_cast<int>(buffer.readableBytes()) - static_cast<int>(sizeof(totalLen))) { return; }
    cout << "Total length: " << totalLen << endl;

    // 读取头部长度
    int headerLen = 0;
    std::memcpy(&headerLen, buffer.peek() + sizeof(totalLen), sizeof(headerLen));

    // 读取数据
    vector<char> header_with_args(totalLen - sizeof(headerLen));
    std::copy(buffer.peek() + sizeof(totalLen) + sizeof(headerLen), buffer.peek() + sizeof(totalLen) + totalLen,
              header_with_args.begin());
    // 消耗读取的数据
    buffer.consume(totalLen + sizeof(totalLen));

    // 解析头部
    louis_rpc::Header header;
    if (!header.ParseFromArray(header_with_args.data(), headerLen)) { return; }
    auto serviceName = header.service_name();
    auto methodName = header.method_name();
    auto paramLen = header.param_len();

    // 获取服务
    auto service = services_.find(serviceName);
    if (service == services_.end()) {
        cout << "Service not found" << endl;
        return;
    }

    // 获取方法
    auto method = service->second.find(methodName);
    if (method == service->second.end()) {
        cout << "Method not found" << endl;
        return;
    }

    // 调用方法
    std::string args(header_with_args.data() + headerLen, paramLen);
    auto response = method->second(args);

    // 发送响应
    uint32_t respLen = ::htonl(response.size());
    conn->send(&respLen, sizeof(respLen));
    conn->send(response.data(), response.size());
}
