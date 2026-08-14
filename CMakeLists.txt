cmake_minimum_required(VERSION 3.25)
project(LouisRPC)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加LouisNet
add_subdirectory(third_party/LouisNet)

find_package(Protobuf CONFIG REQUIRED)

# 添加子目录
add_subdirectory(src)

# 测试相关配置（默认关闭）
option(BUILD_TESTS "Build tests" OFF)

if(BUILD_TESTS)
    enable_testing()
    # 添加测试子目录
    add_subdirectory(tests)
endif()