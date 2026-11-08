add_library(breakwater_engine src/engine.cpp src/scenario.cpp src/search.cpp src/transport.cpp
  include/breakwater/engine.hpp include/breakwater/search.hpp include/breakwater/transport.hpp ../contracts/include/breakwater/types.hpp)
target_include_directories(breakwater_engine PUBLIC include ../contracts/include)
target_link_libraries(breakwater_engine PUBLIC breakwater_options)

add_executable(breakwater_cli src/main.cpp)
target_link_libraries(breakwater_cli PRIVATE breakwater_engine)

add_executable(breakwater_search_example examples/search_example.cpp)
target_link_libraries(breakwater_search_example PRIVATE breakwater_engine)

add_executable(breakwater_transport_example examples/transport_example.cpp)
target_link_libraries(breakwater_transport_example PRIVATE breakwater_engine)

# Executed by science's cross-language checks; this is a private test fixture format.
add_executable(breakwater_rollout_reference examples/rollout_reference.cpp)
target_link_libraries(breakwater_rollout_reference PRIVATE breakwater_engine)

# Private A4 science comparison data, produced by the authoritative transport solver.
add_executable(breakwater_physicsnemo_reference examples/physicsnemo_transport_reference.cpp)
target_link_libraries(breakwater_physicsnemo_reference PRIVATE breakwater_engine)

# Private A9 regime/refinement fixtures, executed by science qualification tools.
add_executable(breakwater_transport_qualification_reference examples/transport_qualification_reference.cpp)
target_link_libraries(breakwater_transport_qualification_reference PRIVATE breakwater_engine)

add_executable(breakwater_benchmark benchmarks/benchmark.cpp)
target_link_libraries(breakwater_benchmark PRIVATE breakwater_engine)
target_compile_definitions(breakwater_benchmark PRIVATE
  BREAKWATER_BENCH_CONFIGURATION="$<CONFIG>"
  BREAKWATER_BENCH_CHECKED=$<BOOL:${BREAKWATER_ENABLE_ASAN}>)

if(BUILD_TESTING)
  add_test(NAME engine.benchmark_correctness COMMAND breakwater_benchmark --verify-only --count 100)
  add_executable(breakwater_engine_tests tests/engine_tests.cpp)
  target_link_libraries(breakwater_engine_tests PRIVATE breakwater_engine)
  add_test(NAME engine.acceptance COMMAND breakwater_engine_tests)
  add_executable(breakwater_search_tests tests/search_tests.cpp)
  target_link_libraries(breakwater_search_tests PRIVATE breakwater_engine)
  add_test(NAME engine.search COMMAND breakwater_search_tests)
  add_executable(breakwater_control_influence_tests tests/control_influence_tests.cpp)
  target_link_libraries(breakwater_control_influence_tests PRIVATE breakwater_engine)
  add_test(NAME engine.control_influence COMMAND breakwater_control_influence_tests)
  add_test(NAME engine.search_example COMMAND breakwater_search_example)
  add_test(NAME engine.search_infeasible_example COMMAND breakwater_search_example --infeasible)
  add_executable(breakwater_transport_tests tests/transport_tests.cpp)
  target_link_libraries(breakwater_transport_tests PRIVATE breakwater_engine)
  add_test(NAME engine.transport COMMAND breakwater_transport_tests)
  add_test(NAME engine.transport_example COMMAND breakwater_transport_example)
endif()
