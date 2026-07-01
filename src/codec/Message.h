#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <variant>

// 消息类型
enum class MsgType : uint8_t {
    Request = 1,
    Response = 2,
};

// RPC 状态码
enum class RpcStatus : uint32_t {
    OK = 0,
    NOT_FOUND = 1,
    INVALID_PARAM = 2,
    INTERNAL_ERROR = 3,
};

// 请求负载
struct RequestPayload {
    std::string serviceName;
    std::string methodName;
    std::vector<char> params;  // 业务参数的序列化结果
};

// 响应负载
struct ResponsePayload {
    RpcStatus statusCode;
    std::vector<char> result;  // 业务结果的序列化结果
};

// 统计消息结构体
struct RpcMessage {
    MsgType msgType;
    uint64_t msgId;

    std::variant<RequestPayload, ResponsePayload> payload;

    bool isRequest() const { return msgType == MsgType::Request; }
    const RequestPayload& getRequestPayload() const { return std::get<RequestPayload>(payload); }
    const ResponsePayload& getResponsePayload() const { return std::get<ResponsePayload>(payload); }
};
