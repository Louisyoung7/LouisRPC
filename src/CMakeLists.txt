file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cc")

# 设置 proto 文件和输出目录
set(PAYLOAD_PROTO ${CMAKE_SOURCE_DIR}/src/proto/payload.proto)
set(PAYLOAD_PROTO_OUT_DIR ${CMAKE_BINARY_DIR}/src/proto)
set(PAYLOAD_PROTO_BIN_DIR ${CMAKE_BINARY_DIR}/src/proto)

# 创建输出目录
file(MAKE_DIRECTORY ${PAYLOAD_PROTO_OUT_DIR})

# 自定义命令生成 payload.pb.h 和 payload.pb.cc
add_custom_command(
    OUTPUT ${PAYLOAD_PROTO_BIN_DIR}/payload.pb.cc ${PAYLOAD_PROTO_BIN_DIR}/payload.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} 
        --proto_path=${CMAKE_SOURCE_DIR}/src/proto
        --cpp_out=${PAYLOAD_PROTO_BIN_DIR}
        ${PAYLOAD_PROTO}
    DEPENDS ${PAYLOAD_PROTO}
    COMMENT "Generating proto files for payload.proto"
    VERBATIM
)

# 将生成的文件加入源列表
list(APPEND SRC_FILES 
    ${PAYLOAD_PROTO_BIN_DIR}/payload.pb.cc
)

add_library(LouisRPCLib STATIC ${SRC_FILES})

target_include_directories(LouisRPCLib PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(LouisRPCLib PRIVATE ${CMAKE_SOURCE_DIR}/third_party/LouisNet/src)
# 添加 protobuf 生成文件的目录 (proto/payload.pb.h 相对于 src 目录)
target_include_directories(LouisRPCLib PRIVATE ${CMAKE_BINARY_DIR}/src/proto)

# 使用 PUBLIC 让 include 目录传播到依赖目标
target_link_libraries(LouisRPCLib PUBLIC LouisNetLib PUBLIC protobuf::libprotobuf PRIVATE pthread)
