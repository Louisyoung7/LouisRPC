#pragma once

#include "proto/header.pb.h"

template <typename Params>
struct Packet {
    uint32_t totalLen;         // headerLen + paramLen
    uint32_t headerLen;        // Header.ByteSizeLong()
    louis_rpc::Header header;  // Protobuf 序列化数据
    uint32_t paramLen;         // Params.ByteSizeLong()
    Params params;             // Protobuf 序列化数据
};