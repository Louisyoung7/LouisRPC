#include <gtest/gtest.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "proto/payload.pb.h"
#include "service/RpcException.h"
#include "service/Service.h"
#include "service/ServiceRegistry.h"

// 业务处理类：复用 test_service.cc 中的测试 handler
class RegistryTestHandler {
   public:
    void echo(const louis_rpc::RpcRequest& req, louis_rpc::RpcResponse& resp) { resp.set_result(req.params()); }

    void willThrow(const louis_rpc::RpcRequest& /*req*/, louis_rpc::RpcResponse& /*resp*/) {
        throw std::runtime_error("handler boom");
    }

    void willThrowRpc(const louis_rpc::RpcRequest& /*req*/, louis_rpc::RpcResponse& /*resp*/) {
        throw RpcException(RpcStatus::INTERNAL_ERROR, "rpc boom");
    }
};

// 辅助函数：构造一个空 RpcRequest 的序列化字节流
static std::vector<char> makeEmptyRequestBytes() {
    louis_rpc::RpcRequest req;
    std::string s = req.SerializeAsString();
    return std::vector<char>(s.begin(), s.end());
}

// 辅助函数：将字符串包装成 RpcRequest 字节流
static std::vector<char> makeRequestBytes(const std::string& params) {
    louis_rpc::RpcRequest req;
    req.set_params(params);
    std::string s = req.SerializeAsString();
    return std::vector<char>(s.begin(), s.end());
}

// ==================== registerService 测试 ====================

TEST(ServiceRegistryRegisterTest, NullService) {
    ServiceRegistry registry;
    EXPECT_FALSE(registry.registerService(nullptr));
}

TEST(ServiceRegistryRegisterTest, EmptyName) {
    ServiceRegistry registry;
    EXPECT_FALSE(registry.registerService(std::make_unique<Service>("")));
}

TEST(ServiceRegistryRegisterTest, RegisterSuccess) {
    ServiceRegistry registry;
    EXPECT_TRUE(registry.registerService(std::make_unique<Service>("CalcService")));
}

TEST(ServiceRegistryRegisterTest, DuplicateName) {
    ServiceRegistry registry;
    EXPECT_TRUE(registry.registerService(std::make_unique<Service>("CalcService")));
    EXPECT_FALSE(registry.registerService(std::make_unique<Service>("CalcService")));
}

TEST(ServiceRegistryRegisterTest, RegisterMultipleDistinctServices) {
    ServiceRegistry registry;
    EXPECT_TRUE(registry.registerService(std::make_unique<Service>("ServiceA")));
    EXPECT_TRUE(registry.registerService(std::make_unique<Service>("ServiceB")));
    EXPECT_TRUE(registry.registerService(std::make_unique<Service>("ServiceC")));
}

// ==================== findService 测试 ====================

TEST(ServiceRegistryFindTest, EmptyRegistry) {
    const ServiceRegistry registry;
    EXPECT_EQ(registry.findService("Any"), nullptr);
}

TEST(ServiceRegistryFindTest, FoundAfterRegister) {
    ServiceRegistry registry;
    registry.registerService(std::make_unique<Service>("MyService"));
    const Service* svc = registry.findService("MyService");
    ASSERT_NE(svc, nullptr);
    EXPECT_EQ(svc->name(), "MyService");
}

TEST(ServiceRegistryFindTest, NonExistentName) {
    ServiceRegistry registry;
    registry.registerService(std::make_unique<Service>("Existing"));
    EXPECT_EQ(registry.findService("NonExistent"), nullptr);
}

TEST(ServiceRegistryFindTest, CaseSensitive) {
    ServiceRegistry registry;
    registry.registerService(std::make_unique<Service>("Service"));
    EXPECT_NE(registry.findService("Service"), nullptr);
    EXPECT_EQ(registry.findService("service"), nullptr);
    EXPECT_EQ(registry.findService("SERVICE"), nullptr);
}

// ==================== dispatch 测试 ====================

TEST(ServiceRegistryDispatchTest, ServiceNotFound) {
    ServiceRegistry registry;
    DispatchResult res = registry.dispatch("NonExistent", "someMethod", {});
    EXPECT_EQ(res.status, RpcStatus::NOT_FOUND);
    // 错误信息应包含服务名
    std::string errMsg(res.result.begin(), res.result.end());
    EXPECT_NE(errMsg.find("NonExistent"), std::string::npos);
}

TEST(ServiceRegistryDispatchTest, MethodNotFound) {
    ServiceRegistry registry;
    auto svc = std::make_unique<Service>("Svc");
    registry.registerService(std::move(svc));

    DispatchResult res = registry.dispatch("Svc", "nonExistentMethod", makeEmptyRequestBytes());
    EXPECT_EQ(res.status, RpcStatus::NOT_FOUND);

    std::string errMsg(res.result.begin(), res.result.end());
    EXPECT_NE(errMsg.find("nonExistentMethod"), std::string::npos);
}

TEST(ServiceRegistryDispatchTest, DispatchSuccess) {
    ServiceRegistry registry;
    auto svc = std::make_unique<Service>("EchoSvc");
    RegistryTestHandler h;
    svc->registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &RegistryTestHandler::echo);
    registry.registerService(std::move(svc));

    DispatchResult res = registry.dispatch("EchoSvc", "echo", makeRequestBytes("hello"));
    EXPECT_EQ(res.status, RpcStatus::OK);

    // 反序列化响应，验证结果
    louis_rpc::RpcResponse resp;
    ASSERT_TRUE(resp.ParseFromArray(res.result.data(), static_cast<int>(res.result.size())));
    EXPECT_EQ(resp.result(), "hello");
}

TEST(ServiceRegistryDispatchTest, DispatchRpcException) {
    ServiceRegistry registry;
    auto svc = std::make_unique<Service>("Svc");
    RegistryTestHandler h;
    svc->registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("boom", &h, &RegistryTestHandler::willThrowRpc);
    registry.registerService(std::move(svc));

    DispatchResult res = registry.dispatch("Svc", "boom", makeEmptyRequestBytes());
    // RpcException 应被捕获，状态码与抛出的异常一致
    EXPECT_EQ(res.status, RpcStatus::INTERNAL_ERROR);

    std::string errMsg(res.result.begin(), res.result.end());
    EXPECT_NE(errMsg.find("rpc boom"), std::string::npos);
}

TEST(ServiceRegistryDispatchTest, DispatchStdException) {
    ServiceRegistry registry;
    auto svc = std::make_unique<Service>("Svc");
    RegistryTestHandler h;
    svc->registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("boom", &h, &RegistryTestHandler::willThrow);
    registry.registerService(std::move(svc));

    DispatchResult res = registry.dispatch("Svc", "boom", makeEmptyRequestBytes());
    // std::exception 应被捕获为 INTERNAL_ERROR
    EXPECT_EQ(res.status, RpcStatus::INTERNAL_ERROR);

    std::string errMsg(res.result.begin(), res.result.end());
    EXPECT_NE(errMsg.find("handler boom"), std::string::npos);
}

// ==================== 多服务隔离测试 ====================

TEST(ServiceRegistryMultiServiceTest, DispatchToDifferentServices) {
    ServiceRegistry registry;
    RegistryTestHandler h;

    auto svcA = std::make_unique<Service>("ServiceA");
    svcA->registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &RegistryTestHandler::echo);
    registry.registerService(std::move(svcA));

    auto svcB = std::make_unique<Service>("ServiceB");
    svcB->registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &RegistryTestHandler::echo);
    registry.registerService(std::move(svcB));

    // 验证两个服务独立工作
    {
        DispatchResult res = registry.dispatch("ServiceA", "echo", makeRequestBytes("fromA"));
        EXPECT_EQ(res.status, RpcStatus::OK);
        louis_rpc::RpcResponse resp;
        ASSERT_TRUE(resp.ParseFromArray(res.result.data(), static_cast<int>(res.result.size())));
        EXPECT_EQ(resp.result(), "fromA");
    }
    {
        DispatchResult res = registry.dispatch("ServiceB", "echo", makeRequestBytes("fromB"));
        EXPECT_EQ(res.status, RpcStatus::OK);
        louis_rpc::RpcResponse resp;
        ASSERT_TRUE(resp.ParseFromArray(res.result.data(), static_cast<int>(res.result.size())));
        EXPECT_EQ(resp.result(), "fromB");
    }
}

// ==================== dispatch 带二进制数据  ====================

TEST(ServiceRegistryDispatchTest, DispatchWithBinaryData) {
    ServiceRegistry registry;
    auto svc = std::make_unique<Service>("BinSvc");
    RegistryTestHandler h;
    svc->registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &RegistryTestHandler::echo);
    registry.registerService(std::move(svc));

    std::string binary;
    binary.push_back(0x00);
    binary.push_back(0x01);
    binary.push_back(static_cast<char>(0xFF));
    binary.push_back('a');

    DispatchResult res = registry.dispatch("BinSvc", "echo", makeRequestBytes(binary));
    EXPECT_EQ(res.status, RpcStatus::OK);

    louis_rpc::RpcResponse resp;
    ASSERT_TRUE(resp.ParseFromArray(res.result.data(), static_cast<int>(res.result.size())));
    EXPECT_EQ(resp.result(), binary);
}