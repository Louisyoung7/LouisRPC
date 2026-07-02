#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

#include "Protocol.h"

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
