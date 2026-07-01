add_executable(caller caller.cc)

target_include_directories(caller PRIVATE ${CMAKE_SOURCE_DIR}/example)

target_link_libraries(caller PRIVATE LouisRPCLib calculator_proto protobuf::libprotobuf pthread)

target_include_directories(caller PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(caller PRIVATE ${CMAKE_SOURCE_DIR}/example)
target_include_directories(caller PRIVATE ${CMAKE_SOURCE_DIR}/third_party/LouisNet/src)
