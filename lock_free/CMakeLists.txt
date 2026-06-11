cmake_minimum_required(VERSION 3.28)

file(GLOB HPP_FILES "*.hpp")

message(${HPP_FILES})

add_executable(bench bench.cpp ${HPP_FILES})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(free_list_test tests/free_list_test.cpp ${HPP_FILES})
target_link_libraries(free_list_test PRIVATE gtest_main)
add_test(NAME free_list_test COMMAND free_list_test)

add_executable(stack_test tests/stack_test.cpp ${HPP_FILES})
target_link_libraries(stack_test PRIVATE gtest_main)
add_test(NAME stack_test COMMAND stack_test)