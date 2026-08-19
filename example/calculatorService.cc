#include <memory>

#include "RpcServer.h"
#include "calculator.pb.h"
#include "net/reactor/EventLoop.h"

using namespace calculator;

class CalculatorService {
   public:
    void Add(const AddRequest& request, AddResponse& response) { response.set_result(request.a() + request.b()); }
};

int main() {
    net::reactor::EventLoop loop;
    RpcServer server(&loop, net::InetAddress("127.0.0.1", 8080));

    CalculatorService calcImpl;
    auto service = std::make_unique<Service>("Calculator");
    service->registerMethod<AddRequest, AddResponse>("Add", &calcImpl, &CalculatorService::Add);
    server.registerService(std::move(service));

    server.start();
    loop.loop();
    return 0;
}