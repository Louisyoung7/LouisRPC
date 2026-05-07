#include "channel.h"

#include <vector>

#include "header.pb.h"

template <typename Req, typename Resp>
void Channel::call(const std::string& serviceName, const std::string& methodName, const Req& request, Resp& response) {
    // 序列化请求参数
    auto argsLen = request.ByteSizeLong();
    std::vector<char> argsStr(argsLen);
    request.SerializeToArray(argsStr.data(), argsLen);

    // 序列化请求头
    louis_rpc::Header header;
    header.set_service_name(serviceName);
    header.set_method_name(methodName);
    header.set_param_len(argsStr.size());
    auto headerLen = header.ByteSizeLong();
    std::vector<char> headerStr(headerLen);
    header.SerializeToArray(headerStr.data(), headerLen);

    // 获取头部长度
    uint32_t headerSize = ::htonl(headerLen);

    // 获取总长度
    uint32_t totalLen = sizeof(headerSize) + headerSize + argsLen;


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

    // 发送请求
    ::write(connFd, &totalLen, sizeof(totalLen));
    ::write(connFd, &headerSize, sizeof(headerSize));
    ::write(connFd, headerStr.data(), headerSize);
    ::write(connFd, argsStr.data(), argsLen);

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