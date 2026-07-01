#pragma once

#include <vector>

#include "base/Buffer.h"
#include "proto/header.pb.h"

static constexpr size_t kLengthFieldSize = sizeof(uint32_t);

class RpcCodec {
   public:
    RpcCodec() = default;
    ~RpcCodec() = default;

    // 编码网络包
    static std::vector<char> encode(const louis_rpc::Header& header, const void* serializedParams, size_t paramSize);

    // 解码网络包
    struct DecodedPacket {
        louis_rpc::Header header;
        std::vector<char> paramData;  // 参数数据
    };
    static std::optional<DecodedPacket> decode(base::Buffer& buffer);
};
