file(GLOB_RECURSE SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cc")

add_library(LouisRPCLib STATIC ${SRC_FILES})

target_include_directories(LouisRPCLib PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(LouisRPCLib PRIVATE LouisNet protobuf::libprotobuf pthread)
