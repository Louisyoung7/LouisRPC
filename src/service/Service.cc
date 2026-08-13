#include "Service.h"

#include "RpcException.h"

// 注册方法，同名方法会覆盖旧的方法
// 模板方法，根据请求和响应的类型自动序列化和反序列化
// ReqType 和 RespType 必须是 google::protobuf::Message 的派生类
template <typename ReqType, typename RespType, typename Class>
void Service::registerMethod(const std::string& methodName, Class* instance,
                             void (Class::*method)(const ReqType& req, RespType& resp)) {
    // 1. 创建 Lambda，捕获 实例指针和成员函数指针
    auto handler = [instance, method](const std::vector<char>& reqBytes) -> std::vector<char> {
        ReqType req;
        RespType resp;

        // 2. 反序列化请求参数
        if (!req.ParseFromArray(reqBytes.data(), reqBytes.size())) {
            throw RpcException(RpcStatus::INVALID_PARAM, "Failed to parse request");
        }

        // 3. 调用业务逻辑
        (instance->*method)(req, resp);

        // 4. 序列化响应结果
        std::string respStr = resp.SerializeAsString();
        return std::vector<char>(respStr.begin(), respStr.end());
    };

    // 5. 注册方法处理函数
    methods_[methodName] = std::move(handler);
}

// 分发调用：方法不存在返回 false；存在则执行 handler 并写入 result，返回 true。
// handler 抛出的异常向上传播，由调用方（Registry）捕获。
bool Service::invoke(const std::string& methodName, const std::vector<char>& params, std::vector<char>& result) const {
    if (!hasMethod(methodName)) return false;

    result = methods_.at(methodName)(params);
    return true;
}