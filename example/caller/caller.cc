#include <arpa/inet.h>
#include <unistd.h>

#include <cassert>
#include <cstring>
#include <iostream>

#include "calculator.pb.h"
#include "net/RpcChannel.h"
#include "net/reactor/EventLoop.h"

using namespace std;
using namespace net;
using namespace net::reactor;
using namespace louis_rpc::calculator;

int main() {
    // 创建EventLoop
    EventLoop loop;

    // 创建对端地址
    InetAddress serverAddr("127.0.0.1", 8080);

    // 创建RpcChannel
    RpcChannel channel(&loop, serverAddr);

    // 创建请求
    AddRequest request;
    request.set_a(-5);
    request.set_b(8);

    AddResponse response;
    channel.call<AddRequest, AddResponse>("calculator", "add", request, response,
                                          [](const AddResponse& resp) { cout << "Result: " << resp.result() << endl; });

    cout << "Result: " << response.result() << endl;
    return 0;
}