# LouisRPC

一个基于 C++17 和 Protocol Buffers 构建的轻量级异步 RPC 框架，底层由 [LouisNet](https://github.com/LouisRPC/LouisNet) 网络库驱动。

## 特性

- **面向服务架构** — 通过服务名和方法名注册服务、分发请求
- **异步与同步调用** — 支持带回调的非阻塞 `callAsync` 以及基于 `std::promise`/`std::future` 的阻塞式 `call`
- **自定义二进制协议** — 紧凑的 11 字节固定头部（魔术字 + 消息类型 + 消息ID + 载荷长度），支持编译期端序控制
- **Protobuf 集成** — 自动完成 protobuf 消息的序列化与反序列化
- **Reactor 模式** — 基于 LouisNet Reactor 模型的事件驱动网络 I/O
- **线程池** — 请求处理卸载至专用线程池，响应在事件循环中发送回客户端
- **超时处理** — 支持单次调用超时及自动取消
- **连接管理** — 自动重连、连接生命周期回调

## 架构

```
┌──────────────────────────────────────────────────────────────┐
│                      LouisRPC (本仓库)                        │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │  RpcServer   │  │  RpcClient   │  │  ServiceRegistry   │  │
│  └──────┬───────┘  └──────┬───────┘  └────────────────────┘  │
│         │                 │                                    │
│  ┌──────┴─────────────────┴───────────────────────────────┐  │
│  │                   RpcCodec / 协议层                     │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │              LouisNet 网络库                             │  │
│  │  TcpServer │ TcpClient │ EventLoop │ Buffer │ Timer     │  │
│  └─────────────────────────────────────────────────────────┘  │
│                                                               │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │              Protocol Buffers (protobuf)                 │  │
│  └─────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

## 依赖

- C++17 编译器（GCC 8+、Clang 10+ 或 MSVC 2019+）
- CMake 3.25+
- [Protocol Buffers](https://github.com/protocolbuffers/protobuf) (protobuf) — 序列化
- [LouisNet](https://github.com/LouisRPC/LouisNet) — 底层网络库（作为 `third_party/LouisNet` 子模块包含）
- Google Test（可选，用于构建测试）

## 构建

```bash
# 克隆时包含子模块
git clone --recursive https://github.com/LouisRPC/LouisRPC.git
cd LouisRPC

# 配置
cmake -B build

# 构建
cmake --build build

# 构建并包含测试
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build
```

## 线路协议

每个 RPC 消息携带一个 11 字节的固定头部：

| 偏移量 | 大小 | 字段       | 描述                         |
|--------|------|------------|------------------------------|
| 0      | 2    | 魔术字     | `0x5250` ("RP")，用于校验    |
| 2      | 1    | 消息类型   | `1` = 请求, `2` = 响应       |
| 3      | 4    | 消息ID     | 唯一消息标识符               |
| 7      | 4    | 载荷长度   | protobuf 载荷的长度          |
| 11     | N    | 载荷       | 序列化后的 `RpcRequest`/`RpcResponse` |

端序可通过 [Protocol.h](src/codec/Protocol.h) 中的 `USE_LITTLE_ENDIAN` 在编译时配置。

## 快速开始

### 1. 使用 protobuf 定义服务

```protobuf
// calculator.proto
syntax = "proto3";
package calculator;

message AddRequest {
    uint32 a = 1;
    uint32 b = 2;
}

message AddResponse {
    uint32 result = 1;
}
```

### 2. 实现服务端

```cpp
#include "RpcServer.h"
#include "calculator.pb.h"
#include "net/reactor/EventLoop.h"

using namespace calculator;

class CalculatorService {
public:
    void Add(const AddRequest& request, AddResponse& response) {
        response.set_result(request.a() + request.b());
    }
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
}
```

### 3. 客户端调用（异步）

```cpp
#include "RpcClient.h"
#include "calculator.pb.h"
#include "net/reactor/EventLoop.h"

using namespace calculator;

int main() {
    net::reactor::EventLoop loop;
    RpcClient client(&loop, net::InetAddress("127.0.0.1", 8080));

    client.setConnectionCallback([&client](bool connected) {
        if (!connected) return;

        AddRequest request;
        request.set_a(1);
        request.set_b(2);
        std::string params = request.SerializeAsString();
        std::vector<char> paramsVec(params.begin(), params.end());

        client.callAsync("Calculator", "Add", paramsVec, 5000,
            [](RpcClient::CallResult result) {
                if (result.ok() && result.status == RpcStatus::OK) {
                    AddResponse response;
                    response.ParseFromArray(result.result.data(), result.result.size());
                    // 使用 response.result()
                }
            });
    });

    client.connect();
    loop.loop();
}
```

### 同步调用

```cpp
RpcClient::CallResult result = client.call("Calculator", "Add", paramsVec, 3000);
```

## 项目结构

```
├── CMakeLists.txt              # 顶层构建配置
├── src/
│   ├── RpcServer.h/.cc         # RPC 服务端实现
│   ├── RpcClient.h/.cc         # RPC 客户端实现
│   ├── codec/
│   │   ├── Protocol.h          # 线路协议定义
│   │   ├── Message.h           # RPC 消息结构体
│   │   └── RpcCodec.h/.cc      # 编解码逻辑
│   ├── service/
│   │   ├── Service.h           # Service 类，含模板方法注册
│   │   ├── ServiceRegistry.h   # 服务注册表，用于请求分发
│   │   └── RpcException.h      # RPC 异常类型
│   └── proto/
│       └── payload.proto       # 线路格式 protobuf 载荷 schema
├── example/
│   ├── calculator.proto        # 示例计算器服务定义
│   ├── calculatorService.cc    # 示例服务端实现
│   ├── client.cc               # 示例客户端实现
│   └── CMakeLists.txt          # 示例构建配置
├── tests/
│   ├── CMakeLists.txt          # 测试构建配置（需要 GTest）
│   ├── test_protocol.cc
│   ├── test_rpc_codec.cc
│   ├── test_service.cc
│   └── test_service_registry.cc
└── third_party/
    └── LouisNet/               # 网络库子模块
```

## API 概览

### RpcServer

| 方法 | 描述 |
|------|------|
| `registerService(Service)` | 注册服务（必须在 `start()` 之前调用） |
| `start()` | 开始监听连接 |

### RpcClient

| 方法 | 描述 |
|------|------|
| `connect()` | 连接到服务器 |
| `disconnect()` | 断开与服务器的连接 |
| `callAsync(service, method, params, timeout, callback)` | 异步 RPC 调用 |
| `call(service, method, params, timeout)` | 同步 RPC 调用 |
| `setConnectionCallback(cb)` | 设置连接生命周期回调 |

### Service

| 方法 | 描述 |
|------|------|
| `registerMethod<Req, Resp>(name, instance, methodPtr)` | 注册方法处理器，自动进行 protobuf 序列化 |

### 错误码 (RpcClient::CallError)

| 错误码 | 含义 |
|--------|------|
| `kNone` | 成功 |
| `kTimeout` | 响应超时 |
| `kNotConnected` | 发起调用时未连接 |
| `kConnectionLost` | 等待响应期间连接丢失 |
| `kSendFailed` | 编码或发送请求失败 |
| `kInvalidUsage` | 在事件循环线程中调用了 `call()` |

## 许可证

MIT