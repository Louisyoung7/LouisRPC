file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cc")

# 设置 proto 文件和输出目录
set(HEADER_PROTO ${CMAKE_SOURCE_DIR}/src/proto/header.proto)
set(HEADER_PROTO_OUT_DIR ${CMAKE_BINARY_DIR}/src/proto)
set(HEADER_PROTO_BIN_DIR ${CMAKE_BINARY_DIR}/src/proto)

# 创建输出目录
file(MAKE_DIRECTORY ${HEADER_PROTO_OUT_DIR})

# 自定义命令生成 header.pb.h 和 header.pb.cc
add_custom_command(
    OUTPUT ${HEADER_PROTO_BIN_DIR}/header.pb.cc ${HEADER_PROTO_BIN_DIR}/header.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} 
        --proto_path=${CMAKE_SOURCE_DIR}/src/proto
        --cpp_out=${HEADER_PROTO_BIN_DIR}
        ${HEADER_PROTO}
    DEPENDS ${HEADER_PROTO}
    COMMENT "Generating proto files for header.proto"
    VERBATIM
)

# 将生成的文件加入源列表
list(APPEND SRC_FILES 
    ${HEADER_PROTO_BIN_DIR}/header.pb.cc
)

add_library(LouisRPCLib STATIC ${SRC_FILES})

target_include_directories(LouisRPCLib PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(LouisRPCLib PRIVATE ${CMAKE_SOURCE_DIR}/third_party/LouisNet/src)
# 添加 protobuf 生成文件的目录 (proto/header.pb.h 相对于 src 目录)
target_include_directories(LouisRPCLib PRIVATE ${CMAKE_BINARY_DIR}/src)

# 使用 PUBLIC 让 include 目录传播到依赖目标
target_link_libraries(LouisRPCLib PUBLIC LouisNetLib PUBLIC protobuf::libprotobuf PRIVATE pthread)
