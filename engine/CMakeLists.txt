add_library(breakwater_engine src/engine.cpp src/scenario.cpp include/breakwater/engine.hpp ../contracts/include/breakwater/types.hpp)
target_include_directories(breakwater_engine PUBLIC include ../contracts/include)
target_link_libraries(breakwater_engine PUBLIC breakwater_options)

add_executable(breakwater_cli src/main.cpp)
target_link_libraries(breakwater_cli PRIVATE breakwater_engine)

if(BUILD_TESTING)
  add_executable(breakwater_engine_tests tests/engine_tests.cpp)
  target_link_libraries(breakwater_engine_tests PRIVATE breakwater_engine)
  add_test(NAME engine.acceptance COMMAND breakwater_engine_tests)
endif()
