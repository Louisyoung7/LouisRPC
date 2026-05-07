#include "codec.h"

// 编码：序列化 Header 和 Message
std::vector<char> RpcCodec::encode(const louis_rpc::Header& header, const google::protobuf::Message& param) {
    // 序列化param
    size_t paramSize = param.ByteSizeLong();
    std::vector<char> paramBuf(paramSize);
    param.SerializeToArray(paramBuf.data(), paramSize);

    // 设置header的param_len
    louis_rpc::Header mutableHeader = header;
    mutableHeader.set_param_len(paramSize);

    // 序列化header
    size_t headerSize = mutableHeader.ByteSizeLong();
    std::vector<char> headerBuf(headerSize);
    mutableHeader.SerializeToArray(headerBuf.data(), headerSize);

    // 计算长度（网络字节序）
    uint32_t totalLen = static_cast<uint32_t>(headerSize + paramSize);
    uint32_t netTotalLen = ::htonl(totalLen);
    uint32_t netHeaderLen = ::htonl(static_cast<uint32_t>(headerSize));

    // 组装数据包
    std::vector<char> packet;
    packet.reserve(kLengthFieldSize * 2 + headerSize + paramSize);

    // 添加长度字段
    packet.insert(packet.end(), netTotalLen, kLengthFieldSize);
    packet.insert(packet.end(), netHeaderLen, kLengthFieldSize);

    // 添加header
    packet.insert(packet.end(), headerBuf.begin(), headerBuf.end());

    // 添加param
    packet.insert(packet.end(), paramBuf.begin(), paramBuf.end());

    return packet;
}
// 解码
std::optional<RpcCodec::DecodedPacket> RpcCodec::decode(base::Buffer& buffer) {
    // 检查是否有足够的字节
    if (buffer.readableBytes() < kLengthFieldSize * 2) { return std::nullopt; }

    // 读取长度字段
    uint32_t totalLen = ::ntohl(*reinterpret_cast<const uint32_t*>(buffer.peek()));
    uint32_t headerLen = ::ntohl(*reinterpret_cast<const uint32_t*>(buffer.peek() + kLengthFieldSize));

    // 检查是否足够的字节
    if (buffer.readableBytes() < totalLen + kLengthFieldSize * 2) { return std::nullopt; }

    // 消耗长度字段
    buffer.consume(kLengthFieldSize * 2);

    // 解析header
    DecodedPacket packet;
    packet.header.ParseFromArray(buffer.peek(), headerLen);
    // 消耗header
    buffer.consume(headerLen);

    // 提取参数数据
    uint32_t paramLen = packet.header.param_len();
    packet.paramData.assign(buffer.peek() + headerLen, paramLen);
    // 消耗参数数据
    buffer.consume(paramLen);

    return packet;
}
