file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cc")

# protobuf_generate 生成 .pb.cc 文件
protobuf_generate(TARGET LouisRPCLib PROTOS ${CMAKE_SOURCE_DIR}/src/proto/header.proto)

add_library(LouisRPCLib STATIC ${SRC_FILES})

target_include_directories(LouisRPCLib PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(LouisRPCLib PRIVATE ${CMAKE_SOURCE_DIR}/third_party/LouisNet/src)

target_link_libraries(LouisRPCLib PRIVATE LouisNetLib protobuf::libprotobuf pthread)
