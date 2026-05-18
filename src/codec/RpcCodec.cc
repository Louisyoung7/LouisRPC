#include "RpcCodec.h"

// 编码网络包：外部负责序列化参数，codec 只处理协议组装
std::vector<char> RpcCodec::encode(const louis_rpc::Header& header, const void* serializedParams, size_t paramSize) {
    // 1.设置header的param_len
    louis_rpc::Header mutableHeader = header;
    mutableHeader.set_param_len(paramSize);

    // 2.序列化header
    size_t headerSize = mutableHeader.ByteSizeLong();
    std::vector<char> headerBuf(headerSize);
    mutableHeader.SerializeToArray(headerBuf.data(), headerSize);

    // 3.计算长度（网络字节序）
    uint32_t totalLen = static_cast<uint32_t>(headerSize + paramSize);
    uint32_t netTotalLen = ::htonl(totalLen);
    uint32_t netHeaderLen = ::htonl(static_cast<uint32_t>(headerSize));

    // 4.构建完整报文
    std::vector<char> packet;
    packet.reserve(kLengthFieldSize * 2 + headerSize + paramSize);

    // 添加长度字段
    packet.insert(packet.end(), reinterpret_cast<const char*>(&netTotalLen),
                  reinterpret_cast<const char*>(&netTotalLen) + kLengthFieldSize);
    packet.insert(packet.end(), reinterpret_cast<const char*>(&netHeaderLen),
                  reinterpret_cast<const char*>(&netHeaderLen) + kLengthFieldSize);

    // 添加header
    packet.insert(packet.end(), headerBuf.begin(), headerBuf.end());

    // 添加param
    if (serializedParams && paramSize > 0) {
        packet.insert(packet.end(), static_cast<const char*>(serializedParams),
                      static_cast<const char*>(serializedParams) + paramSize);
    }

    return packet;
}

// 解码网络包：从网络 Buffer 提取完整报文，解析header和param数据
std::optional<RpcCodec::DecodedPacket> RpcCodec::decode(base::Buffer& buffer) {
    // 1.检查最小长度（两个长度字段）
    if (buffer.readableBytes() < kLengthFieldSize * 2) { return std::nullopt; }

    // 2.读取totalLen和headerLen
    uint32_t totalLen = ::ntohl(*reinterpret_cast<const uint32_t*>(buffer.peek()));
    uint32_t headerLen = ::ntohl(*reinterpret_cast<const uint32_t*>(buffer.peek() + kLengthFieldSize));

    // 3.校验数据完整性
    if (buffer.readableBytes() < kLengthFieldSize * 2 + totalLen) { return std::nullopt; }

    // 4.消耗长度字段
    buffer.consume(kLengthFieldSize * 2);

    DecodedPacket packet;

    // 5.反序列化header
    if (!packet.header.ParseFromArray(buffer.peek(), headerLen)) { return std::nullopt; }
    buffer.consume(headerLen);

    // 6.提取参数原始数据（未反序列化）
    uint32_t paramLen = packet.header.param_len();
    assert(paramLen == totalLen - headerLen);
    packet.paramData.assign(buffer.peek(), buffer.peek() + paramLen);
    buffer.consume(paramLen);

    return packet;
}
