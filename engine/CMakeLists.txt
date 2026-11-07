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

if(BUILD_TESTING)
  add_executable(breakwater_engine_tests tests/engine_tests.cpp)
  target_link_libraries(breakwater_engine_tests PRIVATE breakwater_engine)
  add_test(NAME engine.acceptance COMMAND breakwater_engine_tests)
  add_executable(breakwater_search_tests tests/search_tests.cpp)
  target_link_libraries(breakwater_search_tests PRIVATE breakwater_engine)
  add_test(NAME engine.search COMMAND breakwater_search_tests)
  add_test(NAME engine.search_example COMMAND breakwater_search_example)
  add_test(NAME engine.search_infeasible_example COMMAND breakwater_search_example --infeasible)
  add_executable(breakwater_transport_tests tests/transport_tests.cpp)
  target_link_libraries(breakwater_transport_tests PRIVATE breakwater_engine)
  add_test(NAME engine.transport COMMAND breakwater_transport_tests)
  add_test(NAME engine.transport_example COMMAND breakwater_transport_example)
endif()
