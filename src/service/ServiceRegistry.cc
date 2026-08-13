#include "ServiceRegistry.h"

#include "RpcException.h"

namespace {
std::vector<char> toBytes(const std::string& s) { return std::vector<char>(s.begin(), s.end()); }
}  // namespace

bool ServiceRegistry::registerService(std::unique_ptr<Service> service) {
    if (!service || service->name().empty()) return false;
    // 重名服务不允许注册
    if (findService(service->name()) != nullptr) return false;

    services_.emplace(service->name(), std::move(service));
    return true;
}

Service* ServiceRegistry::findService(const std::string& name) const {
    return services_.find(name) != services_.end() ? services_.at(name).get() : nullptr;
}

DispatchResult ServiceRegistry::dispatch(const std::string& serviceName, const std::string& methodName,
                                         const std::vector<char>& params) const {
    const Service* service = findService(serviceName);
    if (service == nullptr) return {RpcStatus::NOT_FOUND, toBytes("service not found: " + serviceName)};

    try {
        std::vector<char> result;
        if (!service->invoke(methodName, params, result))
            return {RpcStatus::NOT_FOUND, toBytes("method not found: " + methodName)};
        return {RpcStatus::OK, result};
    } catch (const RpcException& e) {
        return {e.status(), toBytes(e.what())};
    } catch (const std::exception& e) {
        return {RpcStatus::INTERNAL_ERROR, toBytes(e.what())};
    } catch (...) {
        return {RpcStatus::INTERNAL_ERROR, toBytes("unknown error: dispatch")};
    }
}
