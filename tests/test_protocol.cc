#include <gtest/gtest.h>

#include <cstring>

#include "codec/Protocol.h"

// 测试 detail 命名空间的读写函数
TEST(ProtocolDetailTest, WriteReadUint16) {
    uint8_t buffer[2];

    // 测试不同的值
    detail::writeUint16(buffer, 0x1234);
    EXPECT_EQ(detail::readUint16(buffer), 0x1234);

    detail::writeUint16(buffer, 0x0000);
    EXPECT_EQ(detail::readUint16(buffer), 0x0000);

    detail::writeUint16(buffer, 0xFFFF);
    EXPECT_EQ(detail::readUint16(buffer), 0xFFFF);

    detail::writeUint16(buffer, kProtocolMagic);
    EXPECT_EQ(detail::readUint16(buffer), kProtocolMagic);
}

TEST(ProtocolDetailTest, WriteReadUint32) {
    uint8_t buffer[4];

    // 测试不同的值
    detail::writeUint32(buffer, 0x12345678);
    EXPECT_EQ(detail::readUint32(buffer), 0x12345678);

    detail::writeUint32(buffer, 0x00000000);
    EXPECT_EQ(detail::readUint32(buffer), 0x00000000);

    detail::writeUint32(buffer, 0xFFFFFFFF);
    EXPECT_EQ(detail::readUint32(buffer), 0xFFFFFFFF);

    detail::writeUint32(buffer, 0x01020304);
    EXPECT_EQ(detail::readUint32(buffer), 0x01020304);
}

TEST(ProtocolDetailTest, WriteReadUint16Boundary) {
    uint8_t buffer[2];

    // 边界值测试
    detail::writeUint16(buffer, 0);
    EXPECT_EQ(detail::readUint16(buffer), 0);

    detail::writeUint16(buffer, 1);
    EXPECT_EQ(detail::readUint16(buffer), 1);

    detail::writeUint16(buffer, UINT16_MAX);
    EXPECT_EQ(detail::readUint16(buffer), UINT16_MAX);
}

TEST(ProtocolDetailTest, WriteReadUint32Boundary) {
    uint8_t buffer[4];

    // 边界值测试
    detail::writeUint32(buffer, 0);
    EXPECT_EQ(detail::readUint32(buffer), 0);

    detail::writeUint32(buffer, 1);
    EXPECT_EQ(detail::readUint32(buffer), 1);

    detail::writeUint32(buffer, UINT32_MAX);
    EXPECT_EQ(detail::readUint32(buffer), UINT32_MAX);
}

// 测试公共接口函数
TEST(ProtocolPublicTest, WriteReadMagic) {
    uint8_t header[kHeaderSize] = {0};

    writeMagic(header);
    EXPECT_TRUE(checkMagic(header));
}

TEST(ProtocolPublicTest, WriteReadMsgType) {
    uint8_t header[kHeaderSize] = {0};

    writeMsgType(header, MsgType::Request);
    EXPECT_EQ(readMsgType(header), MsgType::Request);

    writeMsgType(header, MsgType::Response);
    EXPECT_EQ(readMsgType(header), MsgType::Response);
}

TEST(ProtocolPublicTest, WriteReadMsgId) {
    uint8_t header[kHeaderSize] = {0};

    // 测试不同的消息ID
    writeMsgId(header, 12345);
    EXPECT_EQ(readMsgId(header), 12345);

    writeMsgId(header, 0);
    EXPECT_EQ(readMsgId(header), 0);

    writeMsgId(header, UINT32_MAX);
    EXPECT_EQ(readMsgId(header), UINT32_MAX);

    writeMsgId(header, 999999);
    EXPECT_EQ(readMsgId(header), 999999);
}

TEST(ProtocolPublicTest, WriteReadPayloadLen) {
    uint8_t header[kHeaderSize] = {0};

    // 测试不同的payload长度
    writePayloadLen(header, 1024);
    EXPECT_EQ(readPayloadLen(header), 1024);

    writePayloadLen(header, 0);
    EXPECT_EQ(readPayloadLen(header), 0);

    writePayloadLen(header, kMaxPayloadSize);
    EXPECT_EQ(readPayloadLen(header), kMaxPayloadSize);

    writePayloadLen(header, 1234567);
    EXPECT_EQ(readPayloadLen(header), 1234567);
}

// 测试完整的头部读写
TEST(ProtocolHeaderTest, FullHeaderReadWrite) {
    uint8_t header[kHeaderSize] = {0};

    // 写入所有字段
    writeMagic(header);
    writeMsgType(header, MsgType::Request);
    writeMsgId(header, 12345);
    writePayloadLen(header, 67890);

    // 验证所有字段
    EXPECT_TRUE(checkMagic(header));
    EXPECT_EQ(readMsgType(header), MsgType::Request);
    EXPECT_EQ(readMsgId(header), 12345);
    EXPECT_EQ(readPayloadLen(header), 67890);
}

TEST(ProtocolHeaderTest, HeaderFieldsDoNotOverlap) {
    uint8_t header[kHeaderSize];

    // 初始化为特定模式
    std::memset(header, 0xAA, kHeaderSize);

    // 写入魔数，检查是否影响其他字段
    writeMagic(header);
    EXPECT_TRUE(checkMagic(header));

    // 写入消息类型
    writeMsgType(header, MsgType::Response);
    EXPECT_EQ(readMsgType(header), MsgType::Response);

    // 写入消息ID
    writeMsgId(header, 0xDEADBEEF);
    EXPECT_EQ(readMsgId(header), 0xDEADBEEF);

    // 写入payload长度
    writePayloadLen(header, 0xCAFEBABE);
    EXPECT_EQ(readPayloadLen(header), 0xCAFEBABE);

    // 确认之前写入的字段没有被覆盖
    EXPECT_TRUE(checkMagic(header));
    EXPECT_EQ(readMsgType(header), MsgType::Response);
}

TEST(ProtocolHeaderTest, MultipleFieldsUpdate) {
    uint8_t header[kHeaderSize] = {0};

    // 第一次写入
    writeMagic(header);
    writeMsgType(header, MsgType::Request);
    writeMsgId(header, 100);
    writePayloadLen(header, 200);

    EXPECT_TRUE(checkMagic(header));
    EXPECT_EQ(readMsgType(header), MsgType::Request);
    EXPECT_EQ(readMsgId(header), 100);
    EXPECT_EQ(readPayloadLen(header), 200);

    // 第二次覆盖写入
    writeMsgType(header, MsgType::Response);
    writeMsgId(header, 300);
    writePayloadLen(header, 400);

    EXPECT_EQ(readMsgType(header), MsgType::Response);
    EXPECT_EQ(readMsgId(header), 300);
    EXPECT_EQ(readPayloadLen(header), 400);

    // 魔数应该保持不变
    EXPECT_TRUE(checkMagic(header));
}

// 测试错误的魔数
TEST(ProtocolMagicTest, InvalidMagic) {
    uint8_t header[kHeaderSize] = {0};

    // 未设置魔数时
    EXPECT_FALSE(checkMagic(header));

    // 设置错误的魔数
    detail::writeUint16(header, 0x1234);
    EXPECT_FALSE(checkMagic(header));
}

// 测试端序配置
TEST(ProtocolEndianTest, EndianConfig) {
    // 确认端序配置已定义
    EXPECT_TRUE(USE_LITTLE_ENDIAN || !USE_LITTLE_ENDIAN);
}