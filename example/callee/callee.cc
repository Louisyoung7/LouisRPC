#include <iostream>

#include "provider.h"

using namespace std;

namespace {
int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
}  // namespace

int main() {
    // 创建Provider
    Provider provider("127.0.0.1", 8080);

    // 注册服务
    provider.registerService("add", add);
    provider.registerService("mul", mul);

    // 启动服务端
    provider.start();
    cout << "Provider started" << endl;
    return 0;
}