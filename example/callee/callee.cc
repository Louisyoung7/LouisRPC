#include <iostream>

#include "calculator.pb.h"
#include "provider.h"

using namespace std;
using namespace louis_rpc::calculator;

namespace {
AddResponse add(AddRequest req) {
    AddResponse resp;
    resp.set_result(req.a() + req.b());
    return resp;
}
AddResponse mul(AddRequest req) {
    AddResponse resp;
    resp.set_result(req.a() * req.b());
    return resp;
}
}  // namespace

int main() {
    // 创建Provider
    Provider provider("127.0.0.1", 8080);

    // 注册服务
    provider.registerService<AddRequest, AddResponse>("calculator", "add", add);
    provider.registerService<AddRequest, AddResponse>("calculator", "mul", mul);

    // 启动服务端
    provider.start();
    cout << "Provider started" << endl;
    return 0;
}