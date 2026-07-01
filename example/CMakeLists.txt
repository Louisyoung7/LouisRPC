# 对象库存放生成的 protobuf 源文件
add_library(calculator_proto OBJECT)
protobuf_generate(TARGET calculator_proto PROTOS ${CMAKE_SOURCE_DIR}/example/calculator.proto)
target_link_libraries(calculator_proto PUBLIC protobuf::libprotobuf)

add_subdirectory(callee)
add_subdirectory(caller)
