#include <gtest/gtest.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "proto/payload.pb.h"
#include "service/RpcException.h"
#include "service/Service.h"

// 业务处理类：作为 registerMethod 的实例
// 演示不同的处理逻辑：回显、计算、抛异常
class TestHandler {
   public:
    // 回显方法：把请求 params 原样写入响应 result
    void echo(const louis_rpc::RpcRequest& req, louis_rpc::RpcResponse& resp) { resp.set_result(req.params()); }

    // 加法方法：params 是两个 uint32（小端），返回它们的和
    void add(const louis_rpc::RpcRequest& req, louis_rpc::RpcResponse& resp) {
        if (req.params().size() != sizeof(uint32_t) * 2) {
            resp.set_status_code(static_cast<uint32_t>(RpcStatus::INVALID_PARAM));
            return;
        }
        uint32_t a = 0, b = 0;
        std::memcpy(&a, req.params().data(), sizeof(uint32_t));
        std::memcpy(&b, req.params().data() + sizeof(uint32_t), sizeof(uint32_t));
        uint32_t sum = a + b;
        resp.set_result(&sum, sizeof(sum));
    }

    // 处理时会抛出异常，用于验证异常向上传播
    void willThrow(const louis_rpc::RpcRequest& /*req*/, louis_rpc::RpcResponse& /*resp*/) {
        throw std::runtime_error("handler boom");
    }

    // 处理时会抛出 RpcException，用于验证异常向上传播
    void willThrowRpc(const louis_rpc::RpcRequest& /*req*/, louis_rpc::RpcResponse& /*resp*/) {
        throw RpcException(RpcStatus::INTERNAL_ERROR, "rpc boom");
    }
};

// 辅助函数：把一段字符串包装成 RpcRequest 字节流
static std::vector<char> makeRequestBytes(const std::string& params) {
    louis_rpc::RpcRequest req;
    req.set_params(params);
    std::string s = req.SerializeAsString();
    return std::vector<char>(s.begin(), s.end());
}

// 辅助函数：构造一个空的 RpcRequest 字节流
static std::vector<char> makeEmptyRequestBytes() {
    louis_rpc::RpcRequest req;
    std::string s = req.SerializeAsString();
    return std::vector<char>(s.begin(), s.end());
}

// 辅助函数：把两个 uint32 拼成 params
static std::string packU32Pair(uint32_t a, uint32_t b) {
    std::string s;
    s.append(reinterpret_cast<const char*>(&a), sizeof(a));
    s.append(reinterpret_cast<const char*>(&b), sizeof(b));
    return s;
}

// 辅助函数：解析响应字节流
static louis_rpc::RpcResponse parseResponse(const std::vector<char>& bytes) {
    louis_rpc::RpcResponse resp;
    EXPECT_TRUE(resp.ParseFromArray(bytes.data(), static_cast<int>(bytes.size())));
    return resp;
}

// ==================== 构造与基础信息 ====================

TEST(ServiceBasicTest, NameAccess) {
    Service svc("MyService");
    EXPECT_EQ(svc.name(), "MyService");
}

TEST(ServiceBasicTest, DistinctInstancesHaveDistinctNames) {
    Service svc1("Foo");
    Service svc2("Bar");
    EXPECT_EQ(svc1.name(), "Foo");
    EXPECT_EQ(svc2.name(), "Bar");
    EXPECT_NE(svc1.name(), svc2.name());
}

TEST(ServiceBasicTest, EmptyNameIsAllowed) {
    Service svc("");
    EXPECT_EQ(svc.name(), "");
}

// ==================== hasMethod 测试 ====================

TEST(ServiceHasMethodTest, EmptyServiceHasNoMethods) {
    Service svc("Empty");
    EXPECT_FALSE(svc.hasMethod("foo"));
    EXPECT_FALSE(svc.hasMethod(""));
    EXPECT_FALSE(svc.hasMethod("any"));
}

TEST(ServiceHasMethodTest, ReportsRegisteredMethod) {
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &TestHandler::echo);

    EXPECT_TRUE(svc.hasMethod("echo"));
    EXPECT_FALSE(svc.hasMethod("other"));
    // 方法名区分大小写
    EXPECT_FALSE(svc.hasMethod("Echo"));
    EXPECT_FALSE(svc.hasMethod("ECHO"));
}

TEST(ServiceHasMethodTest, MultipleMethodsAreTracked) {
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("m1", &h, &TestHandler::echo);
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("m2", &h, &TestHandler::add);

    EXPECT_TRUE(svc.hasMethod("m1"));
    EXPECT_TRUE(svc.hasMethod("m2"));
    EXPECT_FALSE(svc.hasMethod("m3"));
}

// ==================== invoke 正常流程 ====================

TEST(ServiceInvokeTest, InvokeEchoMethod) {
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &TestHandler::echo);

    auto reqBytes = makeRequestBytes("hello world");
    std::vector<char> result;
    ASSERT_TRUE(svc.invoke("echo", reqBytes, result));

    auto resp = parseResponse(result);
    EXPECT_EQ(resp.result(), "hello world");
}

TEST(ServiceInvokeTest, InvokeAddMethod) {
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("add", &h, &TestHandler::add);

    auto reqBytes = makeRequestBytes(packU32Pair(7, 35));
    std::vector<char> result;
    ASSERT_TRUE(svc.invoke("add", reqBytes, result));

    auto resp = parseResponse(result);
    ASSERT_EQ(resp.result().size(), sizeof(uint32_t));
    uint32_t sum = 0;
    std::memcpy(&sum, resp.result().data(), sizeof(sum));
    EXPECT_EQ(sum, 42u);
}

TEST(ServiceInvokeTest, InvokeAddInvalidParamSetsStatus) {
    // 业务方法检测到参数非法，通过 status_code 字段返回
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("add", &h, &TestHandler::add);

    auto reqBytes = makeRequestBytes("not enough");
    std::vector<char> result;
    ASSERT_TRUE(svc.invoke("add", reqBytes, result));

    auto resp = parseResponse(result);
    EXPECT_EQ(resp.status_code(), static_cast<uint32_t>(RpcStatus::INVALID_PARAM));
}

TEST(ServiceInvokeTest, InvokeWithEmptyRequest) {
    // 空字节流可以解析为空 proto3 消息，handler 正常返回
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &TestHandler::echo);

    auto reqBytes = makeEmptyRequestBytes();
    std::vector<char> result;
    ASSERT_TRUE(svc.invoke("echo", reqBytes, result));

    auto resp = parseResponse(result);
    EXPECT_EQ(resp.result(), "");
}

TEST(ServiceInvokeTest, InvokeWithBinaryPayloadIncludingNull) {
    // 包含 \0 的二进制数据应被原样回显
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &TestHandler::echo);

    std::string binary;
    binary.push_back(0x00);
    binary.push_back(0x01);
    binary.push_back(0x00);
    binary.push_back(static_cast<char>(0xFF));
    binary.push_back('a');
    auto reqBytes = makeRequestBytes(binary);

    std::vector<char> result;
    ASSERT_TRUE(svc.invoke("echo", reqBytes, result));

    auto resp = parseResponse(result);
    EXPECT_EQ(resp.result(), binary);
}

TEST(ServiceInvokeTest, InvokeDispatchesToCorrectMethod) {
    // 注册多个方法，invoke 应路由到名字对应的方法
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &TestHandler::echo);
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("add", &h, &TestHandler::add);

    // echo
    {
        auto reqBytes = makeRequestBytes("ping");
        std::vector<char> result;
        ASSERT_TRUE(svc.invoke("echo", reqBytes, result));
        EXPECT_EQ(parseResponse(result).result(), "ping");
    }
    // add
    {
        auto reqBytes = makeRequestBytes(packU32Pair(10, 20));
        std::vector<char> result;
        ASSERT_TRUE(svc.invoke("add", reqBytes, result));
        auto resp = parseResponse(result);
        uint32_t sum = 0;
        std::memcpy(&sum, resp.result().data(), sizeof(sum));
        EXPECT_EQ(sum, 30u);
    }
}

// ==================== invoke 错误流程 ====================

TEST(ServiceInvokeTest, InvokeNonExistentMethodReturnsFalse) {
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &TestHandler::echo);

    std::vector<char> result;
    EXPECT_FALSE(svc.invoke("notExist", makeRequestBytes("x"), result));
}

TEST(ServiceInvokeTest, InvokeOnEmptyServiceReturnsFalse) {
    Service svc("S");
    std::vector<char> result;
    EXPECT_FALSE(svc.invoke("any", makeRequestBytes("x"), result));
}

TEST(ServiceInvokeTest, InvokeWithMalformedBytesThrowsRpcException) {
    // 长度声明超过实际数据，ParseFromArray 失败 → 抛出 RpcException
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &TestHandler::echo);

    // 0x0A = field 1 (length-delimited), 0x10 = length 16, 但只给 1 字节数据
    std::vector<char> badBytes = {0x0A, 0x10, 'x'};
    std::vector<char> result;
    EXPECT_THROW(svc.invoke("echo", badBytes, result), RpcException);
}

TEST(ServiceInvokeTest, StdExceptionPropagatesFromHandler) {
    // handler 抛 std::runtime_error，应原样向上传播
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("boom", &h, &TestHandler::willThrow);

    std::vector<char> result;
    EXPECT_THROW(svc.invoke("boom", makeEmptyRequestBytes(), result), std::runtime_error);
}

TEST(ServiceInvokeTest, RpcExceptionPropagatesFromHandler) {
    // handler 抛 RpcException，应原样向上传播（由 Registry 捕获后转成错误响应）
    Service svc("S");
    TestHandler h;
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("boom", &h, &TestHandler::willThrowRpc);

    std::vector<char> result;
    EXPECT_THROW(svc.invoke("boom", makeEmptyRequestBytes(), result), RpcException);
}

// ==================== 方法覆盖 ====================

TEST(ServiceRegisterTest, ReRegisterSameNameOverridesOldHandler) {
    // 注释里明确说明：同名方法会覆盖旧的方法
    Service svc("S");
    TestHandler h;

    // 第一次注册 echo：回显
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &TestHandler::echo);
    {
        auto reqBytes = makeRequestBytes("first");
        std::vector<char> result;
        ASSERT_TRUE(svc.invoke("echo", reqBytes, result));
        EXPECT_EQ(parseResponse(result).result(), "first");
    }

    // 用 add 覆盖 echo
    svc.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("echo", &h, &TestHandler::add);
    EXPECT_TRUE(svc.hasMethod("echo"));

    {
        auto reqBytes = makeRequestBytes(packU32Pair(2, 3));
        std::vector<char> result;
        ASSERT_TRUE(svc.invoke("echo", reqBytes, result));
        auto resp = parseResponse(result);
        ASSERT_EQ(resp.result().size(), sizeof(uint32_t));
        uint32_t sum = 0;
        std::memcpy(&sum, resp.result().data(), sizeof(sum));
        EXPECT_EQ(sum, 5u);
    }
}

// ==================== 多实例隔离 ====================

TEST(ServiceIsolationTest, DifferentServicesHaveIndependentMethodTables) {
    Service svc1("A");
    Service svc2("B");
    TestHandler h;

    svc1.registerMethod<louis_rpc::RpcRequest, louis_rpc::RpcResponse>("m", &h, &TestHandler::echo);
    // svc2 没有注册 m

    EXPECT_TRUE(svc1.hasMethod("m"));
    EXPECT_FALSE(svc2.hasMethod("m"));

    auto reqBytes = makeRequestBytes("x");
    std::vector<char> result;

    EXPECT_TRUE(svc1.invoke("m", reqBytes, result));
    EXPECT_FALSE(svc2.invoke("m", reqBytes, result));
}
