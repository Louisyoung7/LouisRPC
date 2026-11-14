# 设置 proto 文件和输出目录
set(CALCULATOR_PROTO ${PROJECT_SOURCE_DIR}/example/calculator.proto)
set(CALCULATOR_PROTO_OUT_DIR ${PROJECT_BINARY_DIR}/example)

# 创建输出目录
file(MAKE_DIRECTORY ${CALCULATOR_PROTO_OUT_DIR})

# 自定义命令生成 calculator.pb.h 和 calculator.pb.cc
add_custom_command(
    OUTPUT ${CALCULATOR_PROTO_OUT_DIR}/calculator.pb.cc ${CALCULATOR_PROTO_OUT_DIR}/calculator.pb.h
    COMMAND ${Protobuf_PROTOC_EXECUTABLE} 
        --proto_path=${PROJECT_SOURCE_DIR}/example
        --cpp_out=${CALCULATOR_PROTO_OUT_DIR}
        ${CALCULATOR_PROTO}
    DEPENDS ${CALCULATOR_PROTO}
    COMMENT "Generating proto files for calculator.proto"
    VERBATIM
)

# 将生成的文件加入源列表
list(APPEND SRC_FILES 
    ${CALCULATOR_PROTO_OUT_DIR}/calculator.pb.cc
)

add_executable(service calculatorService.cc ${CALCULATOR_PROTO_OUT_DIR}/calculator.pb.cc)
add_executable(client client.cc ${CALCULATOR_PROTO_OUT_DIR}/calculator.pb.cc)

target_include_directories(service PRIVATE ${PROJECT_SOURCE_DIR}/src ${louisnet_SOURCE_DIR}/src ${CALCULATOR_PROTO_OUT_DIR})
target_include_directories(client PRIVATE ${PROJECT_SOURCE_DIR}/src ${louisnet_SOURCE_DIR}/src ${CALCULATOR_PROTO_OUT_DIR})

target_link_libraries(service LouisRPCLib)
target_link_libraries(client LouisRPCLib)


