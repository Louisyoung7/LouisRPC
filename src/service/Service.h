#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

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
