# 生成 calculator.pb.cc
protobuf_generate(TARGET calculator_gen PROTOS ${CMAKE_SOURCE_DIR}/example/calculator.proto)

add_subdirectory(callee)
add_subdirectory(caller)
