#pragma once

#include <unistd.h>

#include <string>

#include "net/InetAddress.h"

class RpcChannel {
   public:
    RpcChannel(const std::string& ip, int port) : addr_(ip, port) {}

    template <typename Req, typename Resp>
    void call(const std::string& serviceName, const std::string& methodName, const Req& request, Resp& response);

   private:
    net::InetAddress addr_;
};