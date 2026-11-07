qt_standard_project_setup(REQUIRES 6.12)

qt_add_library(breakwater_desktop_base STATIC ProjectDocument.cpp ProjectController.cpp ProjectDocument.hpp ProjectController.hpp)
target_include_directories(breakwater_desktop_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(breakwater_desktop_base PUBLIC breakwater_engine Qt6::Quick Qt6::Concurrent)

qt_add_executable(breakwater WIN32 main.cpp)
target_link_libraries(breakwater PRIVATE breakwater_desktop_base Qt6::QuickControls2 Qt6::Quick3D Qt6::Network)
get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
set_target_properties(breakwater PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${_qt_prefix}/bin;%PATH%")
qt_add_qml_module(breakwater URI Breakwater VERSION 1.0
  QML_FILES qml/Main.qml qml/Equipment.qml qml/PipeRoute.qml)

if(BUILD_TESTING)
  qt_add_executable(breakwater_desktop_tests tests/project_tests.cpp)
  target_link_libraries(breakwater_desktop_tests PRIVATE breakwater_desktop_base Qt6::Test)
  target_compile_definitions(breakwater_desktop_tests PRIVATE BREAKWATER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  add_test(NAME desktop.project COMMAND breakwater_desktop_tests)
  add_test(NAME desktop.qml_smoke COMMAND breakwater --smoke)
  set_tests_properties(desktop.qml_smoke PROPERTIES TIMEOUT 30)
  set_tests_properties(desktop.project desktop.qml_smoke PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
endif()

install(TARGETS breakwater RUNTIME DESTINATION bin)
qt_generate_deploy_qml_app_script(TARGET breakwater OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
