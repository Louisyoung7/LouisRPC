#pragma once

#include <memory>
#include <vector>

#include "Service.h"
#include "codec/Protocol.h"

// 依次分发的结果：状态码 + 结果字节
// status == OK 时，result 为序列化后的响应结果
// status != OK 时，status 为错误码，result 为错误信息
struct DispatchResult {
    RpcStatus status = RpcStatus::OK;
    std::vector<char> result;
};

class ServiceRegistry {
   public:
    ServiceRegistry() = default;
    ~ServiceRegistry() = default;

    bool registerService(std::unique_ptr<Service> service);

    Service* findService(const std::string& name) const;

    DispatchResult dispatch(const std::string& serviceName, const std::string& methodName,
                            const std::vector<char>& request) const;

   private:
    std::unordered_map<std::string, std::unique_ptr<Service>> services_; // 服务注册表：服务名 -> 服务实例
};