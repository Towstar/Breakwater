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

# Private E1 analytical numerical increment; no EOG wire/desktop dependency.
add_library(breakwater_eog_analytical src/eog_analytical.cpp include/breakwater/eog_analytical.hpp)
target_include_directories(breakwater_eog_analytical PUBLIC include)
target_link_libraries(breakwater_eog_analytical PUBLIC breakwater_options)
add_executable(breakwater_eog_analytical_example examples/eog_analytical_example.cpp)
target_link_libraries(breakwater_eog_analytical_example PRIVATE breakwater_eog_analytical)

# Private E3 seconds-based native computation; independent of Qt, Python and JSON.
add_library(breakwater_eog_response SHARED src/eog_response.cpp include/breakwater/eog_response.h)
target_compile_definitions(breakwater_eog_response PRIVATE BWEOG_BUILD)
target_include_directories(breakwater_eog_response PUBLIC include)
target_link_libraries(breakwater_eog_response PRIVATE breakwater_options)

# Private extended synthetic plant: persistent events/fleet, independent of UI/wire.
add_library(breakwater_eog_simulation src/eog_simulation.cpp include/breakwater/eog_simulation.hpp)
target_include_directories(breakwater_eog_simulation PUBLIC include)
target_link_libraries(breakwater_eog_simulation PUBLIC breakwater_options)
add_library(breakwater_eog_plant SHARED src/eog_simulation_c.cpp include/breakwater/eog_simulation.h)
target_compile_definitions(breakwater_eog_plant PRIVATE BWES_BUILD)
target_include_directories(breakwater_eog_plant PUBLIC include)
target_link_libraries(breakwater_eog_plant PRIVATE breakwater_eog_simulation)

if(BUILD_TESTING)
  add_executable(breakwater_eog_plant_tests tests/eog_simulation_c_tests.cpp)
  target_link_libraries(breakwater_eog_plant_tests PRIVATE breakwater_eog_plant breakwater_options)
  add_test(NAME engine.eog_plant_abi COMMAND breakwater_eog_plant_tests)
  add_executable(breakwater_eog_simulation_tests tests/eog_simulation_tests.cpp)
  target_link_libraries(breakwater_eog_simulation_tests PRIVATE breakwater_eog_simulation breakwater_eog_response)
  add_test(NAME engine.eog_simulation COMMAND breakwater_eog_simulation_tests)
  add_executable(breakwater_eog_response_tests tests/eog_response_tests.cpp)
  target_link_libraries(breakwater_eog_response_tests PRIVATE breakwater_eog_response breakwater_options)
  add_test(NAME engine.eog_response COMMAND breakwater_eog_response_tests)
  add_executable(breakwater_eog_analytical_tests tests/eog_analytical_tests.cpp)
  target_link_libraries(breakwater_eog_analytical_tests PRIVATE breakwater_eog_analytical)
  add_test(NAME engine.eog_analytical COMMAND breakwater_eog_analytical_tests)
  add_test(NAME engine.eog_analytical_example COMMAND breakwater_eog_analytical_example)
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
