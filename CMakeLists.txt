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
set(TGL_TBB AUTO CACHE STRING "oneTBB engines (level_parallel_tbb, dag_parallel_tbb): AUTO | ON | OFF")
set_property(CACHE TGL_TBB PROPERTY STRINGS AUTO ON OFF)
find_package(Threads REQUIRED)

set(TGL_HAVE_TBB OFF)
if(TGL_TBB STREQUAL "ON")
  find_package(TBB REQUIRED)
  set(TGL_HAVE_TBB ON)
elseif(TGL_TBB STREQUAL "AUTO" AND TGL_SANITIZER STREQUAL "thread")
  # A prebuilt libtbb is not instrumented, so TSan reports false races inside TBB's own
  # task allocator. Use -DTGL_TBB=ON only with a TBB that was itself built with TSan.
  message(STATUS "TGL_TBB=AUTO is off under TSan (libtbb is not instrumented)")
elseif(TGL_TBB STREQUAL "AUTO")
  find_package(TBB QUIET)
  if(TBB_FOUND)
    set(TGL_HAVE_TBB ON)
  endif()
elseif(NOT TGL_TBB STREQUAL "OFF")
  message(FATAL_ERROR "TGL_TBB must be AUTO, ON or OFF")
endif()
set(TGL_HAVE_TBB ${TGL_HAVE_TBB} CACHE INTERNAL "TBB engines compiled in (read by run_suite.py)")
if(TGL_HAVE_TBB)
  message(STATUS "oneTBB ${TBB_VERSION}: TBB engines enabled")
else()
  message(STATUS "oneTBB not used: TBB engines disabled")
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wno-missing-field-initializers)
# No fused multiply-add contraction: Clang contracts a*b+c by default on arm64 (Apple
# Silicon), which changes rounding and breaks bit-identical results across platforms.
add_compile_options(-ffp-contract=off)
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
if(TGL_HAVE_TBB)
  target_link_libraries(tgl PUBLIC TBB::tbb)
  target_compile_definitions(tgl PUBLIC TGL_HAVE_TBB=1)
endif()

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
if(TGL_HAVE_TBB)
  add_test(NAME bench_smoke_tbb
    COMMAND timing_graph_bench --topology reconvergent --nodes 3000 --layers 30 --repetitions 1 --warmup 0
            --algorithm parallel --threads 1,2 --barrier tbb,tbb_dag --output ${CMAKE_BINARY_DIR}/bench_smoke_tbb.csv)
  set_tests_properties(bench_smoke_tbb PROPERTIES LABELS integration)
endif()
