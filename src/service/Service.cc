#include "Service.h"

// 分发调用：方法不存在返回 false；存在则执行 handler 并写入 result，返回 true。
// handler 抛出的异常向上传播，由调用方（Registry）捕获。
bool Service::invoke(const std::string& methodName, const std::vector<char>& params, std::vector<char>& result) const {
    if (!hasMethod(methodName)) return false;

    result = methods_.at(methodName)(params);
    return true;
}