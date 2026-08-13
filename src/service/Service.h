#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "RpcException.h"

// 方法处理函数的类型：字节进，字节出
using MethodHandler = std::function<std::vector<char>(const std::vector<char>&)>;

class Service {
   public:
    explicit Service(std::string name) : name_(std::move(name)) {}
    virtual ~Service() = default;

    // 获取服务名
    const std::string& name() const { return name_; }

    template <typename ReqType, typename RespType, typename Class>
    void registerMethod(const std::string& methodName, Class* instance,
                        void (Class::*method)(const ReqType& req, RespType& resp));

    // 检查服务是否注册了指定方法
    bool hasMethod(const std::string& methodName) const { return methods_.find(methodName) != methods_.end(); }

    bool invoke(const std::string& methodName, const std::vector<char>& params, std::vector<char>& result) const;

   private:
    std::string name_;                                        // 服务名
    std::unordered_map<std::string, MethodHandler> methods_;  // 方法名 -》 处理函数的映射
};

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
