cmake_minimum_required(VERSION 3.25)
project(Breakwater VERSION 0.1.0 LANGUAGES CXX)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Qt-generated utility targets repeat long project names in MSBuild tracking
# paths. Keep tracking enabled while avoiding MAX_PATH in nested checkouts.
if(CMAKE_GENERATOR MATCHES "^Visual Studio" AND
   NOT "${CMAKE_VS_GLOBALS}" MATCHES "(^|;)TLogLocation=")
  list(APPEND CMAKE_VS_GLOBALS
    "TLogLocation=${CMAKE_BINARY_DIR}/.t/$(ProjectGuid)/$(Platform)/$(Configuration)/")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(BREAKWATER_BUILD_DESKTOP "Build the Qt Quick 3D desktop workbench" ON)
option(BREAKWATER_ENABLE_ASAN "Instrument owned native targets with AddressSanitizer" OFF)

add_library(breakwater_options INTERFACE)
if(MSVC)
  target_compile_options(breakwater_options INTERFACE /W4 /permissive- /EHsc /fp:precise /utf-8)
  if(BREAKWATER_ENABLE_ASAN)
    target_compile_options(breakwater_options INTERFACE /fsanitize=address)
    target_link_options(breakwater_options INTERFACE /INCREMENTAL:NO)
  endif()
else()
  target_compile_options(breakwater_options INTERFACE -Wall -Wextra -Wpedantic -Wconversion)
  if(BREAKWATER_ENABLE_ASAN)
    target_compile_options(breakwater_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(breakwater_options INTERFACE -fsanitize=address,undefined)
  endif()
endif()

include(CTest)
if(BUILD_TESTING)
  # Compiled by headless/native jobs too; this header must never require Qt.
  add_library(breakwater_eog_dto_compile OBJECT contracts/eog/tests/dto_compile.cpp)
  target_include_directories(breakwater_eog_dto_compile PRIVATE contracts/include)
  target_link_libraries(breakwater_eog_dto_compile PRIVATE breakwater_options)
endif()
add_subdirectory(engine)
if(BREAKWATER_BUILD_DESKTOP)
  find_package(Qt6 6.12 REQUIRED COMPONENTS Quick QuickControls2 Quick3D Network Concurrent Sql Test)
  add_subdirectory(desktop)
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT breakwater)
else()
  set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT breakwater_cli)
endif()
