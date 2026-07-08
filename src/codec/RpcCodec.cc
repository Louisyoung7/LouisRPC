#include "RpcCodec.h"

#include "Message.h"
#include "Protocol.h"
#include "proto/payload.pb.h"

ssize_t RpcCodec::encode(const RpcMessage& msg, base::Buffer& output) {
    louis_rpc::RpcRequest pbReq;
    louis_rpc::RpcResponse pbResp;

    // 声明Message基类指针，用于指向请求或响应消息
    const google::protobuf::Message* pbMsg = nullptr;
    size_t payloadSize = 0;

    // 根据消息类型进行序列化
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

    // 确定具体类型后获取序列化后的大小
    payloadSize = pbMsg->ByteSizeLong();

    if (payloadSize > kMaxPayloadSize) { return -1; }

    // 写入固定头部
    uint8_t header[kHeaderSize];
    writeMagic(header);
    writeMsgType(header, msg.msgType);
    writeMsgId(header, msg.msgId);
    writePayloadLen(header, static_cast<uint32_t>(payloadSize));
    output.append(header, kHeaderSize);

    // 直接在Buffer中写入序列化后的Payload
    char* payloadPtr = output.writerIt();
    if (!pbMsg->SerializeToArray(payloadPtr, static_cast<int>(payloadSize))) { return -1; }

    // 写入完成，增加写索引
    output.hasWritten(payloadSize);

    return kHeaderSize + payloadSize;
}

ssize_t RpcCodec::decode(base::Buffer& input, RpcMessage& out_msg) {
    
}
