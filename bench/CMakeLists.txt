include(FetchContent)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)

FetchContent_Declare(benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.9.1
  GIT_SHALLOW ON
  SYSTEM
)
FetchContent_MakeAvailable(benchmark)

add_executable(gb_bench bench_main.cpp)
target_link_libraries(gb_bench PRIVATE gbcore benchmark::benchmark_main)
target_compile_definitions(gb_bench PRIVATE GB_ROM_DIR="${CMAKE_SOURCE_DIR}/tests/roms")
gb_set_warnings(gb_bench)
