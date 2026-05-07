#include <arpa/inet.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

#include "calculator.pb.h"

using namespace std;

int sendRequest(int fd, const std::string& serviceName, int a, int b, sockaddr_in serverAddr) {
    if (connect(fd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("connect");
        return 1;
    }

    cout << "Send request to " << inet_ntoa(serverAddr.sin_addr) << ":" << ntohs(serverAddr.sin_port) << endl;

    // 构建请求
    calculator::AddRequest request;
    request.set_name(serviceName);
    request.set_a(a);
    request.set_b(b);

    auto requestSize = request.ByteSizeLong();

    // 发送请求
    vector<char> requestData(requestSize + sizeof(int));
    // 写入请求长度
    memcpy(requestData.data(), &requestSize, sizeof(int));
    // 写入序列化的请求数据
    request.SerializeToArray(requestData.data() + sizeof(int), requestSize);
    if (::send(fd, requestData.data(), requestSize + sizeof(int), 0) < 0) {
        perror("send");
        return 1;
    }

    cout << "Send request done" << endl;

    // 接收响应
    vector<char> response(sizeof(int));
    if (::recv(fd, response.data(), sizeof(int), 0) < 0) {
        perror("recv");
        return 1;
    }
    int result = 0;
    memcpy(&result, response.data(), sizeof(int));
    return result;
}

int main() {
    int callerFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (callerFd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(8080);

    int result = sendRequest(callerFd, "mul", -5, 8, serverAddr);
    cout << "Result: " << result << endl;
    ::close(callerFd);
    return 0;
}