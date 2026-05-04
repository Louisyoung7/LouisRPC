add_executable(caller caller.cc)
target_link_libraries(caller LouisRPCLib)
target_include_directories(caller PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(caller PRIVATE ${CMAKE_SOURCE_DIR}/third_party/LouisNet/src)
