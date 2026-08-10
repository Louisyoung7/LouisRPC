#include "RpcCodec.h"

#include "Message.h"
#include "Protocol.h"
#include "proto/payload.pb.h"

ssize_t RpcCodec::encode(const RpcMessage& msg, base::Buffer& output) {
    louis_rpc::RpcRequest pbReq;
    louis_rpc::RpcResponse pbResp;

    // 1.声明Message基类指针，用于指向请求或响应消息
    const google::protobuf::Message* pbMsg = nullptr;
    size_t payloadSize = 0;

    // 2.根据消息类型进行序列化
    if (msg.isRequest()) {
        const auto& req = msg.getRequestPayload();
        pbReq.set_service_name(req.serviceName);
        pbReq.set_method_name(req.methodName);
        pbReq.set_params(req.params.data(), req.params.size());
        pbMsg = &pbReq;
    } else {
        const auto& resp = msg.getResponsePayload();
        pbResp.set_status_code(static_cast<uint32_t>(resp.statusCode));
        pbResp.set_result(resp.result.data(), resp.result.size());
        pbMsg = &pbResp;
    }

    // 3.确定具体类型后获取序列化后的大小
    payloadSize = pbMsg->ByteSizeLong();

    if (payloadSize > kMaxPayloadSize) { return -1; }

    // 4.写入固定头部
    uint8_t header[kHeaderSize];
    writeMagic(header);
    writeMsgType(header, msg.msgType);
    writeMsgId(header, msg.msgId);
    writePayloadLen(header, static_cast<uint32_t>(payloadSize));
    output.append(header, kHeaderSize);

    // 5.写入序列化后的Payload
    // 确保buffer有足够的空间写入payload
    output.ensureWritableBytes(payloadSize);
    char* payloadPtr = output.writerIt();
    if (!pbMsg->SerializeToArray(payloadPtr, static_cast<int>(payloadSize))) { return -1; }

    // 写入完成，增加写索引
    output.hasWritten(payloadSize);

    return kHeaderSize + payloadSize;
}

ssize_t RpcCodec::decode(base::Buffer& input, RpcMessage& out_msg) {
    // 检查头部数据是否足够
    if (input.readableBytes() < kHeaderSize) { return 0; }

    // 1.获取头部数据指针
    const uint8_t* header = reinterpret_cast<const uint8_t*>(input.peek());

    // 2.检查魔数
    if (!checkMagic(header)) { return -1; }

    // 3.获取消息类型、消息ID与Payload长度
    const MsgType msgType = readMsgType(header);
    const uint32_t msgId = readMsgId(header);
    const uint32_t payloadLen = readPayloadLen(header);

    // 检查Payload长度是否有效
    if (payloadLen > kMaxPayloadSize) { return -1; }

    // 计算总消息长度
    const size_t totalSize = kHeaderSize + payloadLen;

    // 检查消息数据是否足够
    if (input.readableBytes() < totalSize) { return 0; }

    // 获取消息数据指针
    const char* payloadPtr = input.peek() + kHeaderSize;

    // 4.根据消息类型反序列化Payload并构建负载结构体
    if (msgType == MsgType::Request) {
        // 反序列化请求消息
        louis_rpc::RpcRequest pbReq;
        if (!pbReq.ParseFromArray(payloadPtr, static_cast<int>(payloadLen))) { return -1; }
        // 构建请求负载
        RequestPayload req;
        req.serviceName = pbReq.service_name();
        req.methodName = pbReq.method_name();
        req.params.assign(pbReq.params().data(), pbReq.params().data() + pbReq.params().size());
        // 构建请求消息
        out_msg.msgType = msgType;
        out_msg.msgId = msgId;
        out_msg.payload = std::move(req);

    } else if (msgType == MsgType::Response) {
        // 反序列化响应消息
        louis_rpc::RpcResponse pbResp;
        if (!pbResp.ParseFromArray(payloadPtr, static_cast<int>(payloadLen))) { return -1; }
        // 构建响应负载
        ResponsePayload resp;
        resp.statusCode = static_cast<RpcStatus>(pbResp.status_code());
        resp.result.assign(pbResp.result().data(), pbResp.result().data() + pbResp.result().size());
        // 构建响应消息
        out_msg.msgType = msgType;
        out_msg.msgId = msgId;
        out_msg.payload = std::move(resp);
    } else {
        return -1;
    }

    // 5.消耗已读数据
    input.consume(totalSize);

    return totalSize;
}
