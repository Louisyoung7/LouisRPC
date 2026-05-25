add_executable(caller caller.cc ${CMAKE_BINARY_DIR}/example/calculator.pb.cc)

target_include_directories(caller PRIVATE ${CMAKE_SOURCE_DIR}/example)

target_link_libraries(caller LouisRPCLib protobuf::libprotobuf pthread)

target_include_directories(caller PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(caller PRIVATE ${CMAKE_SOURCE_DIR}/example)
target_include_directories(caller PRIVATE ${CMAKE_SOURCE_DIR}/third_party/LouisNet/src)
