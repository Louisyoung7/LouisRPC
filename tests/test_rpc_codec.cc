#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "codec/Message.h"
#include "codec/Protocol.h"
#include "codec/RpcCodec.h"
#include "proto/payload.pb.h"

// 辅助函数：创建请求消息
RpcMessage createRequestMessage(uint32_t msgId, const std::string& serviceName, const std::string& methodName,
                                const std::vector<char>& params) {
    RpcMessage msg;
    msg.msgType = MsgType::Request;
    msg.msgId = msgId;
    msg.payload = RequestPayload{serviceName, methodName, params};
    return msg;
}

// 辅助函数：创建响应消息
RpcMessage createResponseMessage(uint32_t msgId, RpcStatus statusCode, const std::vector<char>& result) {
    RpcMessage msg;
    msg.msgType = MsgType::Response;
    msg.msgId = msgId;
    msg.payload = ResponsePayload{statusCode, result};
    return msg;
}

// ==================== Encode 测试 ====================

TEST(RpcCodecEncodeTest, EncodeRequestMessage) {
    base::Buffer output;

    std::vector<char> params = {'t', 'e', 's', 't'};
    RpcMessage msg = createRequestMessage(12345, "TestService", "TestMethod", params);

    ssize_t ret = RpcCodec::encode(msg, output);

    // 编码应成功，返回值应等于头部大小 + payload 大小
    EXPECT_GT(ret, 0);
    EXPECT_EQ(ret, static_cast<ssize_t>(output.readableBytes()));

    // 验证头部字段
    const uint8_t* header = reinterpret_cast<const uint8_t*>(output.peek());
    EXPECT_TRUE(checkMagic(header));
    EXPECT_EQ(readMsgType(header), MsgType::Request);
    EXPECT_EQ(readMsgId(header), 12345u);

    // 消费头部
    output.consume(kHeaderSize);
    // 验证 payload 可以被解析为 RpcRequest
    louis_rpc::RpcRequest pbReq;
    EXPECT_TRUE(pbReq.ParseFromArray(output.peek(), static_cast<int>(output.readableBytes())));
    EXPECT_EQ(pbReq.service_name(), "TestService");
    EXPECT_EQ(pbReq.method_name(), "TestMethod");
    EXPECT_EQ(pbReq.params().size(), 4u);
}

TEST(RpcCodecEncodeTest, EncodeResponseMessage) {
    base::Buffer output;

    std::vector<char> result = {'O', 'K'};
    RpcMessage msg = createResponseMessage(67890, RpcStatus::OK, result);

    ssize_t ret = RpcCodec::encode(msg, output);

    EXPECT_GT(ret, 0);

    // 验证头部字段
    const uint8_t* header = reinterpret_cast<const uint8_t*>(output.peek());
    EXPECT_TRUE(checkMagic(header));
    EXPECT_EQ(readMsgType(header), MsgType::Response);
    EXPECT_EQ(readMsgId(header), 67890u);

    // 消费头部
    output.consume(kHeaderSize);
    // 验证 payload 可以被解析为 RpcResponse
    louis_rpc::RpcResponse pbResp;
    EXPECT_TRUE(pbResp.ParseFromArray(output.peek(), static_cast<int>(output.readableBytes())));
    EXPECT_EQ(pbResp.status_code(), static_cast<uint32_t>(RpcStatus::OK));
    EXPECT_EQ(pbResp.result().size(), 2u);
}

TEST(RpcCodecEncodeTest, EncodeRequestWithEmptyParams) {
    base::Buffer output;

    std::vector<char> emptyParams;
    RpcMessage msg = createRequestMessage(1, "Service", "Method", emptyParams);

    ssize_t ret = RpcCodec::encode(msg, output);

    EXPECT_GT(ret, 0);

    // 验证头部和 payload
    const uint8_t* header = reinterpret_cast<const uint8_t*>(output.peek());
    EXPECT_TRUE(checkMagic(header));
    EXPECT_EQ(readMsgId(header), 1u);
}

TEST(RpcCodecEncodeTest, EncodeResponseWithDifferentStatusCodes) {
    base::Buffer output;

    // 测试不同的状态码
    RpcStatus statuses[] = {RpcStatus::OK, RpcStatus::NOT_FOUND, RpcStatus::INVALID_PARAM, RpcStatus::INTERNAL_ERROR};

    for (size_t i = 0; i < sizeof(statuses) / sizeof(statuses[0]); ++i) {
        output.consumeAll();

        std::vector<char> result;
        RpcMessage msg = createResponseMessage(static_cast<uint32_t>(i), statuses[i], result);

        ssize_t ret = RpcCodec::encode(msg, output);

        EXPECT_GT(ret, 0);

        const uint8_t* header = reinterpret_cast<const uint8_t*>(output.peek());
        EXPECT_TRUE(checkMagic(header));
        EXPECT_EQ(readMsgType(header), MsgType::Response);
        EXPECT_EQ(readMsgId(header), static_cast<uint32_t>(i));
    }
}

TEST(RpcCodecEncodeTest, EncodeMultipleMessagesToSameBuffer) {
    base::Buffer output;

    std::vector<char> params1 = {'a', 'b'};
    RpcMessage msg1 = createRequestMessage(1, "S1", "M1", params1);
    ssize_t ret1 = RpcCodec::encode(msg1, output);

    std::vector<char> params2 = {'c', 'd', 'e'};
    RpcMessage msg2 = createRequestMessage(2, "S2", "M2", params2);
    ssize_t ret2 = RpcCodec::encode(msg2, output);

    EXPECT_GT(ret1, 0);
    EXPECT_GT(ret2, 0);

    // 验证 buffer 中包含两条消息
    size_t totalLen = static_cast<size_t>(ret1) + static_cast<size_t>(ret2);
    EXPECT_EQ(output.readableBytes(), totalLen);
}

// ==================== Decode 测试 ====================

TEST(RpcCodecDecodeTest, DecodeInsufficientData) {
    base::Buffer input;
    RpcMessage out_msg;

    // 空缓冲区
    ssize_t ret = RpcCodec::decode(input, out_msg);
    EXPECT_EQ(ret, 0);

    // 只有部分头部数据
    input.append("abc", 3);
    ret = RpcCodec::decode(input, out_msg);
    EXPECT_EQ(ret, 0);

    // 完整头部但缺少 payload
    uint8_t header[kHeaderSize] = {0};
    writeMagic(header);
    writeMsgType(header, MsgType::Request);
    writeMsgId(header, 1);
    writePayloadLen(header, 100);  // 需要 100 字节 payload，但实际没有

    input.consumeAll();
    input.append(header, kHeaderSize);
    ret = RpcCodec::decode(input, out_msg);
    EXPECT_EQ(ret, 0);
}

TEST(RpcCodecDecodeTest, DecodeInvalidMagic) {
    base::Buffer input;
    RpcMessage out_msg;

    // 构造一个无效魔数的头部
    uint8_t header[kHeaderSize] = {0};
    detail::writeUint16(header, 0x1234);  // 错误的魔数
    writeMsgType(header, MsgType::Request);
    writeMsgId(header, 1);
    writePayloadLen(header, 0);

    input.append(header, kHeaderSize);

    ssize_t ret = RpcCodec::decode(input, out_msg);
    EXPECT_EQ(ret, -1);
}

TEST(RpcCodecDecodeTest, DecodeInvalidMsgType) {
    base::Buffer input;
    RpcMessage out_msg;

    // 构造一个无效消息类型的头部
    uint8_t header[kHeaderSize] = {0};
    writeMagic(header);
    header[kMsgTypeOffset] = 99;  // 无效的消息类型
    writeMsgId(header, 1);
    writePayloadLen(header, 0);

    input.append(header, kHeaderSize);

    ssize_t ret = RpcCodec::decode(input, out_msg);
    EXPECT_EQ(ret, -1);
}

TEST(RpcCodecDecodeTest, DecodePayloadExceedsMaxSize) {
    base::Buffer input;
    RpcMessage out_msg;

    // 构造一个 payload 长度超过限制的头部
    uint8_t header[kHeaderSize] = {0};
    writeMagic(header);
    writeMsgType(header, MsgType::Request);
    writeMsgId(header, 1);
    writePayloadLen(header, static_cast<uint32_t>(kMaxPayloadSize + 1));

    input.append(header, kHeaderSize);

    ssize_t ret = RpcCodec::decode(input, out_msg);
    EXPECT_EQ(ret, -1);
}

// ==================== 往返测试 (Round-Trip) ====================

TEST(RpcCodecRoundTripTest, RequestRoundTrip) {
    base::Buffer buffer;

    // 创建原始请求消息
    std::vector<char> originalParams = {'p', 'a', 'r', 'a', 'm', 's'};
    RpcMessage originalMsg = createRequestMessage(99999, "MyService", "MyMethod", originalParams);

    // 编码
    ssize_t encodeRet = RpcCodec::encode(originalMsg, buffer);
    ASSERT_GT(encodeRet, 0);

    // 解码
    RpcMessage decodedMsg;
    ssize_t decodeRet = RpcCodec::decode(buffer, decodedMsg);
    ASSERT_GT(decodeRet, 0);

    // 验证解码后的消息
    EXPECT_EQ(decodedMsg.msgType, MsgType::Request);
    EXPECT_EQ(decodedMsg.msgId, 99999u);
    EXPECT_TRUE(decodedMsg.isRequest());

    const RequestPayload& req = decodedMsg.getRequestPayload();
    EXPECT_EQ(req.serviceName, "MyService");
    EXPECT_EQ(req.methodName, "MyMethod");
    EXPECT_EQ(req.params, originalParams);
}

TEST(RpcCodecRoundTripTest, ResponseRoundTrip) {
    base::Buffer buffer;

    // 创建原始响应消息
    std::vector<char> originalResult = {'r', 'e', 's', 'u', 'l', 't'};
    RpcMessage originalMsg = createResponseMessage(88888, RpcStatus::NOT_FOUND, originalResult);

    // 编码
    ssize_t encodeRet = RpcCodec::encode(originalMsg, buffer);
    ASSERT_GT(encodeRet, 0);

    // 解码
    RpcMessage decodedMsg;
    ssize_t decodeRet = RpcCodec::decode(buffer, decodedMsg);
    ASSERT_GT(decodeRet, 0);

    // 验证解码后的消息
    EXPECT_EQ(decodedMsg.msgType, MsgType::Response);
    EXPECT_EQ(decodedMsg.msgId, 88888u);
    EXPECT_FALSE(decodedMsg.isRequest());

    const ResponsePayload& resp = decodedMsg.getResponsePayload();
    EXPECT_EQ(resp.statusCode, RpcStatus::NOT_FOUND);
    EXPECT_EQ(resp.result, originalResult);
}

TEST(RpcCodecRoundTripTest, EmptyParamsRoundTrip) {
    base::Buffer buffer;

    std::vector<char> emptyParams;
    RpcMessage originalMsg = createRequestMessage(1, "S", "M", emptyParams);

    ssize_t encodeRet = RpcCodec::encode(originalMsg, buffer);
    ASSERT_GT(encodeRet, 0);

    RpcMessage decodedMsg;
    ssize_t decodeRet = RpcCodec::decode(buffer, decodedMsg);
    ASSERT_GT(decodeRet, 0);

    const RequestPayload& req = decodedMsg.getRequestPayload();
    EXPECT_EQ(req.serviceName, "S");
    EXPECT_EQ(req.methodName, "M");
    EXPECT_TRUE(req.params.empty());
}

TEST(RpcCodecRoundTripTest, EmptyResultRoundTrip) {
    base::Buffer buffer;

    std::vector<char> emptyResult;
    RpcMessage originalMsg = createResponseMessage(1, RpcStatus::OK, emptyResult);

    ssize_t encodeRet = RpcCodec::encode(originalMsg, buffer);
    ASSERT_GT(encodeRet, 0);

    RpcMessage decodedMsg;
    ssize_t decodeRet = RpcCodec::decode(buffer, decodedMsg);
    ASSERT_GT(decodeRet, 0);

    const ResponsePayload& resp = decodedMsg.getResponsePayload();
    EXPECT_EQ(resp.statusCode, RpcStatus::OK);
    EXPECT_TRUE(resp.result.empty());
}

TEST(RpcCodecRoundTripTest, BinaryDataRoundTrip) {
    base::Buffer buffer;

    // 包含二进制数据（包括 \0）
    std::vector<char> binaryParams = {0x00, 0x01, 0x02, static_cast<char>(0xFF), static_cast<char>(0xFE),
                                      0x00, 'a',  'b'};
    RpcMessage originalMsg = createRequestMessage(100, "BinService", "BinMethod", binaryParams);

    ssize_t encodeRet = RpcCodec::encode(originalMsg, buffer);
    ASSERT_GT(encodeRet, 0);

    RpcMessage decodedMsg;
    ssize_t decodeRet = RpcCodec::decode(buffer, decodedMsg);
    ASSERT_GT(decodeRet, 0);

    const RequestPayload& req = decodedMsg.getRequestPayload();
    EXPECT_EQ(req.params, binaryParams);
}

TEST(RpcCodecRoundTripTest, MultipleMessagesRoundTrip) {
    base::Buffer buffer;

    // 编码多条消息
    std::vector<RpcMessage> originalMessages;
    for (int i = 0; i < 5; ++i) {
        std::vector<char> params = {static_cast<char>('a' + i)};
        RpcMessage msg =
            createRequestMessage(static_cast<uint32_t>(i), "Service", "Method" + std::to_string(i), params);
        ssize_t ret = RpcCodec::encode(msg, buffer);
        ASSERT_GT(ret, 0);
        originalMessages.push_back(msg);
    }

    // 解码并验证每条消息
    for (int i = 0; i < 5; ++i) {
        RpcMessage decodedMsg;
        ssize_t ret = RpcCodec::decode(buffer, decodedMsg);
        ASSERT_GT(ret, 0);

        EXPECT_EQ(decodedMsg.msgType, MsgType::Request);
        EXPECT_EQ(decodedMsg.msgId, static_cast<uint32_t>(i));

        const RequestPayload& req = decodedMsg.getRequestPayload();
        EXPECT_EQ(req.serviceName, "Service");
        EXPECT_EQ(req.methodName, "Method" + std::to_string(i));
    }

    // 所有消息都应该被消耗
    EXPECT_EQ(buffer.readableBytes(), 0u);
}

TEST(RpcCodecRoundTripTest, LargePayloadRoundTrip) {
    base::Buffer buffer;

    // 创建较大的 payload (1KB)
    std::vector<char> largeParams(1024, 'X');
    RpcMessage originalMsg = createRequestMessage(1, "LargeService", "LargeMethod", largeParams);

    ssize_t encodeRet = RpcCodec::encode(originalMsg, buffer);
    ASSERT_GT(encodeRet, 0);

    RpcMessage decodedMsg;
    ssize_t decodeRet = RpcCodec::decode(buffer, decodedMsg);
    ASSERT_GT(decodeRet, 0);

    const RequestPayload& req = decodedMsg.getRequestPayload();
    EXPECT_EQ(req.params.size(), 1024u);
    EXPECT_EQ(req.params, largeParams);
}

// ==================== 边界条件测试 ====================

TEST(RpcCodecEdgeCaseTest, MaxMsgId) {
    base::Buffer buffer;

    std::vector<char> params;
    RpcMessage msg = createRequestMessage(UINT32_MAX, "S", "M", params);

    ssize_t encodeRet = RpcCodec::encode(msg, buffer);
    ASSERT_GT(encodeRet, 0);

    RpcMessage decodedMsg;
    ssize_t decodeRet = RpcCodec::decode(buffer, decodedMsg);
    ASSERT_GT(decodeRet, 0);

    EXPECT_EQ(decodedMsg.msgId, UINT32_MAX);
}

TEST(RpcCodecEdgeCaseTest, ZeroMsgId) {
    base::Buffer buffer;

    std::vector<char> params;
    RpcMessage msg = createRequestMessage(0, "S", "M", params);

    ssize_t encodeRet = RpcCodec::encode(msg, buffer);
    ASSERT_GT(encodeRet, 0);

    RpcMessage decodedMsg;
    ssize_t decodeRet = RpcCodec::decode(buffer, decodedMsg);
    ASSERT_GT(decodeRet, 0);

    EXPECT_EQ(decodedMsg.msgId, 0u);
}

TEST(RpcCodecEdgeCaseTest, LongServiceName) {
    base::Buffer buffer;

    // 较长的服务名和方法名
    std::string longServiceName(100, 'S');
    std::string longMethodName(100, 'M');

    std::vector<char> params;
    RpcMessage msg = createRequestMessage(1, longServiceName, longMethodName, params);

    ssize_t encodeRet = RpcCodec::encode(msg, buffer);
    ASSERT_GT(encodeRet, 0);

    RpcMessage decodedMsg;
    ssize_t decodeRet = RpcCodec::decode(buffer, decodedMsg);
    ASSERT_GT(decodeRet, 0);

    const RequestPayload& req = decodedMsg.getRequestPayload();
    EXPECT_EQ(req.serviceName, longServiceName);
    EXPECT_EQ(req.methodName, longMethodName);
}

// ==================== 所有状态码测试 ====================

TEST(RpcCodecStatusCodeTest, AllStatusCodes) {
    base::Buffer buffer;

    RpcStatus statuses[] = {RpcStatus::OK, RpcStatus::NOT_FOUND, RpcStatus::INVALID_PARAM, RpcStatus::INTERNAL_ERROR};

    for (auto status : statuses) {
        buffer.consumeAll();

        std::vector<char> result = {'d', 'a', 't', 'a'};
        RpcMessage msg = createResponseMessage(1, status, result);

        ssize_t encodeRet = RpcCodec::encode(msg, buffer);
        ASSERT_GT(encodeRet, 0);

        RpcMessage decodedMsg;
        ssize_t decodeRet = RpcCodec::decode(buffer, decodedMsg);
        ASSERT_GT(decodeRet, 0);

        const ResponsePayload& resp = decodedMsg.getResponsePayload();
        EXPECT_EQ(resp.statusCode, status);
    }
}