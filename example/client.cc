#include "RpcClient.h"
#include "calculator.pb.h"
#include "codec/Protocol.h"
#include "log/Logger.h"
#include "net/reactor/EventLoop.h"

using namespace calculator;

int main() {
    net::reactor::EventLoop loop;
    RpcClient client(&loop, net::InetAddress("127.0.0.1", 8080));

    client.setConnectionCallback([&client](bool connected) {
        if (!connected) {
            info("Connection lost.");
            return;
        } else {
            AddRequest request;
            request.set_a(1);
            request.set_b(2);
            std::string params = request.SerializeAsString();
            std::vector<char> paramsVec(params.begin(), params.end());

            client.callAsync("Calculator", "Add", paramsVec, 5000, [](RpcClient::CallResult result) {
                if (result.ok() && result.status == RpcStatus::OK) {
                    AddResponse response;
                    if (response.ParseFromArray(result.result.data(), result.result.size())) {
                        info("Add result: {}", response.result());
                    } else {
                        error("Add response parse failed.");
                    }
                } else {
                    error("Add failed. error: {}", static_cast<int>(result.error));
                }
            });
        }
    });

    client.connect();
    loop.loop();

    return 0;
}