#include "RpcChannel.h"

#include <vector>

#include "codec/RpcCodec.h"
#include "proto/header.pb.h"

template <typename Req, typename Resp>
void RpcChannel::call(const std::string& serviceName, const std::string& methodName, const Req& request,
                      Resp& response) {
    // 设置头部的服务名和方法名
    louis_rpc::Header header;
    header.set_service_name(serviceName);
    header.set_method_name(methodName);

    // 序列化请求
    std::vector<char> reqData = RpcCodec::encode(header, request);

    // 建立连接
    int connFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (connFd < 0) {
        perror("socket");
        return;
    }
    if (::connect(connFd, addr_.getSockaddr(), sizeof(struct sockaddr_in)) < 0) {
        perror("connect");
        ::close(connFd);
        return;
    }

    // 接收响应
    std::string responseStr;
    int readSize = 0;
    while ((readSize = ::read(connFd, &responseStr.back(), responseStr.size() - responseStr.size())) > 0) {
        responseStr.resize(responseStr.size() + readSize);
    }
    if (readSize < 0) {
        perror("read");
        ::close(connFd);
        return;
    }

    // 反序列化响应
    if (!response.ParseFromString(responseStr)) {
        perror("ParseFromString");
        return;
    }
    responseStr.clear();
    ::close(connFd);
}