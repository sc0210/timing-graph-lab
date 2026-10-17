cmake_minimum_required(VERSION 3.20)
project(timing_graph_lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(TGL_SANITIZER "" CACHE STRING "Sanitizer build: '' | address (ASan+UBSan) | thread (TSan)")
option(TGL_NATIVE "Compile with -march=native (results become CPU-specific)" OFF)
find_package(Threads REQUIRED)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
if(TGL_NATIVE)
  add_compile_options(-march=native)
endif()
if(TGL_SANITIZER STREQUAL "address")
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  add_link_options(-fsanitize=address,undefined)
elseif(TGL_SANITIZER STREQUAL "thread")
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
elseif(NOT TGL_SANITIZER STREQUAL "")
  message(FATAL_ERROR "TGL_SANITIZER must be '', address or thread")
endif()

add_library(tgl
  src/graph/edge_list.cpp
  src/graph/csr_graph.cpp
  src/graph/generators.cpp
  src/sta/incremental.cpp)
target_include_directories(tgl PUBLIC include)
target_link_libraries(tgl PUBLIC Threads::Threads)

add_executable(timing_graph_bench src/benchmark/timing_graph_bench.cpp)
target_link_libraries(timing_graph_bench PRIVATE tgl)
string(TOUPPER "${CMAKE_BUILD_TYPE}" _bt)
target_compile_definitions(timing_graph_bench PRIVATE
  TGL_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
  TGL_CXX_FLAGS="${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${_bt}}")

enable_testing()
foreach(suite unit property differential regression)
  add_executable(test_${suite} tests/${suite}/test_${suite}.cpp)
  target_link_libraries(test_${suite} PRIVATE tgl)
  add_test(NAME ${suite} COMMAND test_${suite})
  set_tests_properties(${suite} PROPERTIES LABELS ${suite})
endforeach()

add_test(NAME bench_smoke
  COMMAND timing_graph_bench --topology reconvergent --nodes 3000 --layers 30 --repetitions 1 --warmup 0
          --threads 1,2 --output ${CMAKE_BINARY_DIR}/bench_smoke.csv)
set_tests_properties(bench_smoke PROPERTIES LABELS integration)
