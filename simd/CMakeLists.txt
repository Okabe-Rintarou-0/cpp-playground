cmake_minimum_required(VERSION 3.28)

include(FetchContent)
FetchContent_Declare(
  highway
  GIT_REPOSITORY https://github.com/google/highway.git
  GIT_TAG        7588bd5b12b6c93192dbf12afcf066a8eada96ea
)
set(HWY_ENABLE_CONTRIB OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(HWY_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(highway)

file(GLOB HPP_FILES "*.hpp")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(simd_bench simd_bench.cpp ${HPP_FILES})
target_link_libraries(simd_bench PRIVATE hwy)
target_compile_options(simd_bench PRIVATE /arch:AVX2)

add_executable(simd_bench_noopt simd_bench.cpp ${HPP_FILES})
target_link_libraries(simd_bench_noopt PRIVATE hwy)
target_compile_options(simd_bench_noopt PRIVATE /Od)
target_compile_definitions(simd_bench_noopt PRIVATE NO_OPT_BUILD=1)

enable_testing()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/simd_test.cpp")
  add_executable(simd_test tests/simd_test.cpp ${HPP_FILES})
  target_link_libraries(simd_test PRIVATE hwy gtest_main)
  add_test(NAME simd_test COMMAND simd_test)
endif()
