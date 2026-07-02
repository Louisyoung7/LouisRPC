#pragma once

#include <cstdint>
#include <cstring>

// 魔数 "RP" (0x5250)，用于快速校验连接合法性及粘包检测
constexpr uint16_t kProtocolMagic = 0x5250;

// 消息类型
enum class MsgType : uint8_t {
    Request = 1,
    Response = 2,
};

// RPC 状态码
enum class RpcStatus : uint8_t {
    OK = 0,
    NOT_FOUND = 1,
    INVALID_PARAM = 2,
    INTERNAL_ERROR = 3,
};

// Payload 最大允许大小
constexpr size_t kMaxPayloadSize = 64 * 1024 * 1024;  // 64MB

// 各字段在Header中的字节偏移量
constexpr size_t kMagicOffset = 0;
constexpr size_t kMsgTypeOffset = 2;
constexpr size_t kMsgIdOffset = 3;
constexpr size_t kPayloadLenOffset = 7;

// 固定头部总长度：Magic(2) + MsgType(1) + MsgId(4) + PayloadLen(4) = 11 Bytes
constexpr size_t kHeaderHeaderSize = 11;

// 编译期端序开关
// true  = 小端序 (x86/ARM 原生，零转换开销)
// false = 大端序 (传统网络字节序，跨异构平台安全)
// 默认为小端序
constexpr bool USE_LITTLE_ENDIAN = true;

namespace detail {
// 写入16位无符号整数到缓冲区
inline void writeUint16(uint8_t* buf, uint16_t val) {
    // 小端序直接写入，大端序需要交换字节序
    if constexpr (USE_LITTLE_ENDIAN) {
        std::memcpy(buf, &val, sizeof(val));
    } else {
        buf[0] = static_cast<uint8_t>(val >> 8);
        buf[1] = static_cast<uint8_t>(val);
    }
}

// 写入32位无符号整数到缓冲区
inline void writeUint32(uint8_t* buf, uint32_t val) {
    if constexpr (USE_LITTLE_ENDIAN) {
        std::memcpy(buf, &val, sizeof(val));
    } else {
        buf[0] = static_cast<uint8_t>(val >> 24);
        buf[1] = static_cast<uint8_t>(val >> 16);
        buf[2] = static_cast<uint8_t>(val >> 8);
        buf[3] = static_cast<uint8_t>(val);
    }
}

// 从缓冲区读取16位无符号整数
inline uint16_t readUint16(const uint8_t* buf) {
    // 小端序直接读取，大端序需要交换字节序
    if constexpr (USE_LITTLE_ENDIAN) {
        uint16_t val;
        std::memcpy(&val, buf, sizeof(val));
        return val;
    } else {
        return static_cast<uint16_t>(buf[0]) << 8 | static_cast<uint16_t>(buf[1]);
    }
}

// 从缓冲区读取32位无符号整数
inline uint32_t readUint32(const uint8_t* buf) {
    if constexpr (USE_LITTLE_ENDIAN) {
        uint32_t val;
        std::memcpy(&val, buf, sizeof(val));
        return val;
    } else {
        return static_cast<uint32_t>(buf[0]) << 24 | static_cast<uint32_t>(buf[1]) << 16 |
               static_cast<uint32_t>(buf[2]) << 8 | static_cast<uint32_t>(buf[3]);
    }
}
}  // namespace detail

inline void writeMagic(uint8_t* header) { detail::writeUint16(header + kMagicOffset, kProtocolMagic); }

inline void writeMsgType(uint8_t* header, MsgType msgType) { header[kMsgTypeOffset] = static_cast<uint8_t>(msgType); }

inline void writeMsgId(uint8_t* header, uint32_t msgId) { detail::writeUint32(header + kMsgIdOffset, msgId); }

inline void writePayloadLen(uint8_t* header, uint32_t payloadLen) {
    detail::writeUint32(header + kPayloadLenOffset, payloadLen);
}

inline bool checkMagic(uint8_t* header) { return detail::readUint16(header + kMagicOffset) == kProtocolMagic; }

inline MsgType readMsgType(const uint8_t* header) { return static_cast<MsgType>(header[kMsgTypeOffset]); }

inline uint32_t readMsgId(const uint8_t* header) { return detail::readUint32(header + kMsgIdOffset); }

inline uint32_t readPayloadLen(const uint8_t* header) { return detail::readUint32(header + kPayloadLenOffset); }
