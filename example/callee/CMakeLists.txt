add_executable(callee callee.cc)
target_link_libraries(callee LouisRPCLib)
target_include_directories(callee PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(callee PRIVATE ${CMAKE_SOURCE_DIR}/third_party/LouisNet/src)
