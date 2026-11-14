find_package(GTest CONFIG REQUIRED)

file(GLOB_RECURSE TEST_SOURCES "${PROJECT_SOURCE_DIR}/tests/*.cc")

add_executable(LouisRPCTests ${TEST_SOURCES})

target_link_libraries(LouisRPCTests GTest::gtest_main LouisRPCLib)

target_include_directories(LouisRPCTests PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_include_directories(LouisRPCTests PRIVATE "${louisnet_SOURCE_DIR}/src")
target_include_directories(LouisRPCTests PRIVATE "${PROJECT_BINARY_DIR}/src")

include(GoogleTest)
gtest_discover_tests(LouisRPCTests)