#pragma once

#include <vector>

#include "base/Buffer.h"
#include "google/protobuf/message.h"
#include "proto/header.pb.h"

class RpcCodec {
   public:
    RpcCodec() = default;
    ~RpcCodec() = default;

    // 编码：序列化 Header 和 Message
    static std::vector<char> encode(const louis_rpc::Header& header, const google::protobuf::Message& param);

    // 解码
    struct DecodedPacket {
        louis_rpc::Header header;
        std::string paramData;  // 参数数据
    };
    static std::optional<DecodedPacket> decode(base::Buffer& buffer);

   private:
    static constexpr size_t kLengthFieldSize = sizeof(uint32_t);
};
