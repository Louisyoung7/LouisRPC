#pragma once

#include "Message.h"
#include "base/Buffer.h"

static constexpr size_t kLengthFieldSize = sizeof(uint32_t);

class RpcCodec {
   public:
    // 编码：结构化消息 -> 字节流
    // 返回写入字节数，失败返回 -1
    static ssize_t encode(const RpcMessage& msg, base::Buffer& output);

    // 解码：字节流 -> 结构化消息
    // >0: 成功，返回消耗的字节数
    //  0: 数据不足，需要继续读取
    // <0: 协议错误（magic错/长度超限等），应断连
    static ssize_t decode(base::Buffer& input, RpcMessage& out_msg);
};
