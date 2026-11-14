cmake_minimum_required(VERSION 3.28)
project(LouisRPC)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 测试相关配置（默认关闭）
option(BUILD_TESTS "Build tests" OFF)

include(FetchContent)

# 通过 FetchContent 引入 LouisNet（替代原来的 git submodule）
FetchContent_Declare(
    LouisNet
    GIT_REPOSITORY https://github.com/Louisyoung7/LouisNet.git
    GIT_TAG v1.3.0
    GIT_SHALLOW TRUE
)

# LouisNet 顶层同样使用 BUILD_TESTS，且 Debug 下会强制打开；
# 这里临时用普通变量屏蔽，避免一并配置它自己的测试（额外依赖 GTest）
set(_louisrpc_build_tests ${BUILD_TESTS})
set(BUILD_TESTS OFF)
FetchContent_MakeAvailable(LouisNet)
# 还原 LouisRPC 自己的开关（LouisNet 可能已改写同名 cache 变量）
unset(BUILD_TESTS)
set(BUILD_TESTS ${_louisrpc_build_tests} CACHE BOOL "Build tests" FORCE)
unset(_louisrpc_build_tests)

find_package(Protobuf CONFIG REQUIRED)

# 添加子目录
add_subdirectory(src)
add_subdirectory(example)

if(BUILD_TESTS)
    enable_testing()
    # 添加测试子目录
    add_subdirectory(tests)
endif()
