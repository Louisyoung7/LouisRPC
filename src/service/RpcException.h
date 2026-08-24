#pragma once

#include <exception>
#include <string>

#include "codec/Protocol.h"

class RpcException : public std::exception {
   public:
    explicit RpcException(const RpcStatus status, const std::string msg = "") : status_(status), msg_(std::move(msg)) {}

    RpcStatus status() const noexcept { return status_; }

    const char* what() const noexcept override { return msg_.c_str(); }

   private:
    RpcStatus status_;
    std::string msg_;
};