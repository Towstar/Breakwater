qt_standard_project_setup(REQUIRES 6.12)

qt_add_library(breakwater_desktop_base STATIC ProjectDocument.cpp ProjectController.cpp ProjectDocument.hpp ProjectController.hpp
  quality/PipeParameters.hpp quality/PipeParameters.cpp)
target_include_directories(breakwater_desktop_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(breakwater_desktop_base PUBLIC breakwater_engine Qt6::Quick Qt6::Concurrent)

qt_add_library(breakwater_agent STATIC
  agent/ModelTransport.hpp agent/NemotronClient.hpp agent/NemotronClient.cpp
  agent/CredentialSettings.hpp agent/CredentialSettings.cpp
  agent/SimulationGateway.hpp agent/SimulationGateway.cpp
  agent/AgentOrchestrator.hpp agent/AgentOrchestrator.cpp
  agent/AgentController.hpp agent/AgentController.cpp
  history/SessionRepository.hpp history/SessionRepository.cpp
  history/HistoryController.hpp history/HistoryController.cpp
  history/SessionRecorder.hpp history/SessionRecorder.cpp
  quality/PipeTransportController.hpp quality/PipeTransportController.cpp
  quality/PipeTransportEvidence.hpp quality/PipeTransportEvidence.cpp
  science/PythonWorkerClient.hpp science/PythonWorkerClient.cpp)
target_sources(breakwater_agent PRIVATE
  history/RehearsalEvidenceGuard.hpp history/RehearsalArchiveCoordinator.hpp
  science/WorkerProtocol.hpp science/WorkerProtocol.cpp
  science/StorageScienceEvidence.hpp science/StorageScienceEvidence.cpp
  science/StorageScienceAnalysis.hpp science/StorageScienceAnalysis.cpp
  science/AgentSensitivityClient.hpp science/AgentSensitivityClient.cpp
  science/StorageScienceController.hpp science/StorageScienceController.cpp)
target_include_directories(breakwater_agent PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(breakwater_agent PUBLIC breakwater_desktop_base Qt6::Network Qt6::Concurrent Qt6::Sql)
target_compile_definitions(breakwater_agent PRIVATE BREAKWATER_WORKER_PATH="${PROJECT_SOURCE_DIR}/science/worker.py")
target_compile_definitions(breakwater_agent PRIVATE BREAKWATER_DEVELOPMENT_BIN_DIR="$<TARGET_FILE_DIR:breakwater>")
qt_add_executable(breakwater_nemotron_smoke agent/live_smoke.cpp)
target_link_libraries(breakwater_nemotron_smoke PRIVATE breakwater_agent)

qt_add_executable(breakwater WIN32 main.cpp)
target_link_libraries(breakwater PRIVATE breakwater_agent Qt6::QuickControls2 Qt6::Quick3D)
get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
set_target_properties(breakwater PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${_qt_prefix}/bin;%PATH%")
qt_add_qml_module(breakwater URI Breakwater VERSION 1.0
  QML_FILES qml/Main.qml qml/Equipment.qml qml/PipeRoute.qml qml/AgentPanel.qml qml/TrajectoryChart.qml qml/PipeTransportPanel.qml qml/StorageSciencePanel.qml qml/SourceFeedDialog.qml)

# Qt 6.12's statistics target can precede the executable's QML cache generation
# in a fresh parallel Visual Studio build. Qt creates that target at root deferral.
if(CMAKE_GENERATOR MATCHES "^Visual Studio")
  function(_breakwater_order_qml_aotstats)
    if(TARGET module_breakwater_aotstats_target)
      add_dependencies(module_breakwater_aotstats_target breakwater)
    endif()
  endfunction()
  cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}"
    CALL _breakwater_order_qml_aotstats)
endif()

if(BUILD_TESTING)
  qt_add_executable(breakwater_source_feed_qml_tests tests/source_feed_qml_tests.cpp)
  target_link_libraries(breakwater_source_feed_qml_tests PRIVATE breakwater_agent Qt6::QuickControls2 Qt6::Quick3D Qt6::Test)
  target_compile_definitions(breakwater_source_feed_qml_tests PRIVATE BREAKWATER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  add_test(NAME desktop.source_feed_qml COMMAND breakwater_source_feed_qml_tests
    -o "${CMAKE_CURRENT_BINARY_DIR}/source_feed_qml-results.txt,txt")
  set_tests_properties(desktop.source_feed_qml PROPERTIES TIMEOUT 30
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  qt_add_executable(breakwater_desktop_tests tests/project_tests.cpp)
  target_link_libraries(breakwater_desktop_tests PRIVATE breakwater_desktop_base Qt6::Test)
  target_compile_definitions(breakwater_desktop_tests PRIVATE BREAKWATER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  add_test(NAME desktop.project COMMAND breakwater_desktop_tests)
  qt_add_executable(breakwater_pipe_parameters_tests tests/pipe_parameters_tests.cpp)
  target_link_libraries(breakwater_pipe_parameters_tests PRIVATE breakwater_desktop_base Qt6::Test)
  add_test(NAME desktop.pipe_parameters COMMAND breakwater_pipe_parameters_tests)
  set_tests_properties(desktop.pipe_parameters PROPERTIES TIMEOUT 30
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  foreach(_suite nemotron_client agent_gateway agent_orchestrator session_repository history_controller python_worker pipe_transport pipe_transport_evidence storage_worker storage_science science_evidence agent_science)
    set(_suite_source "tests/${_suite}_tests.cpp")
    if(_suite STREQUAL "science_evidence")
      # Qt's generated VS import-scanner paths must fit Windows MAX_PATH.
      set(_suite_source "tests/storage_science_evidence_tests.cpp")
    endif()
    qt_add_executable(breakwater_${_suite}_tests ${_suite_source})
    target_link_libraries(breakwater_${_suite}_tests PRIVATE breakwater_agent Qt6::Test)
    target_compile_definitions(breakwater_${_suite}_tests PRIVATE BREAKWATER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME desktop.${_suite} COMMAND breakwater_${_suite}_tests -o "${CMAKE_CURRENT_BINARY_DIR}/${_suite}-results.txt,txt")
    set_tests_properties(desktop.${_suite} PROPERTIES TIMEOUT 60
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  endforeach()
  if(DEFINED ENV{BREAKWATER_PYTHON} AND NOT DEFINED Python3_EXECUTABLE)
    set(Python3_EXECUTABLE "$ENV{BREAKWATER_PYTHON}")
  endif()
  find_package(Python3 3.12 QUIET COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    file(TO_CMAKE_PATH "${Python3_EXECUTABLE}" _breakwater_test_python)
    foreach(_worker_suite python_worker storage_worker storage_science science_evidence agent_science agent_orchestrator)
      target_compile_definitions(breakwater_${_worker_suite}_tests PRIVATE BREAKWATER_TEST_PYTHON="${_breakwater_test_python}")
    endforeach()
    execute_process(COMMAND "${Python3_EXECUTABLE}" -c "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec('torch') else 1)"
      RESULT_VARIABLE _breakwater_torch_probe OUTPUT_QUIET ERROR_QUIET TIMEOUT 10)
    if(_breakwater_torch_probe EQUAL 0)
      foreach(_science_size normal compact derivatives)
        set(_science_args --science-check --smoke --science-report "${CMAKE_CURRENT_BINARY_DIR}/science-${_science_size}.json"
          --history-db "${CMAKE_CURRENT_BINARY_DIR}/science-${_science_size}.sqlite")
        if(_science_size STREQUAL "compact")
          list(APPEND _science_args --compact-ui --science-example bad)
        elseif(_science_size STREQUAL "derivatives")
          list(APPEND _science_args --science-derivatives
            --science-control source:source_a:curtail --science-control route:bypass:flow)
        endif()
        add_test(NAME desktop.storage_science_${_science_size} COMMAND breakwater ${_science_args})
        set_tests_properties(desktop.storage_science_${_science_size} PROPERTIES TIMEOUT 120
          ENVIRONMENT "BREAKWATER_PYTHON=${_breakwater_test_python}"
          ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
      endforeach()
    endif()
  else()
    foreach(_worker_suite python_worker storage_worker storage_science science_evidence agent_science agent_orchestrator)
      target_compile_definitions(breakwater_${_worker_suite}_tests PRIVATE BREAKWATER_TEST_PYTHON="")
    endforeach()
  endif()
  set_tests_properties(desktop.storage_worker desktop.storage_science desktop.science_evidence desktop.agent_science desktop.agent_orchestrator PROPERTIES TIMEOUT 180)
  # These functional suites launch many real CPU interpreters; storage_worker
  # measured 170.71 s for 121 cases on the development host.
  set_tests_properties(desktop.storage_worker desktop.storage_science PROPERTIES TIMEOUT 360)
  add_test(NAME desktop.qml_smoke COMMAND breakwater --smoke)
  set_tests_properties(desktop.qml_smoke PROPERTIES TIMEOUT 30)
  # Reopen an actual native export produced by the controller suite, rather than
  # treating a successful empty-scene render as an evidence-import check.
  set(_pipe_evidence_dir "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts")
  set_tests_properties(desktop.pipe_transport PROPERTIES FIXTURES_SETUP pipe_evidence
    ENVIRONMENT "BREAKWATER_TEST_ARTIFACT_DIR=${_pipe_evidence_dir}")
  set_tests_properties(desktop.science_evidence PROPERTIES FIXTURES_SETUP science_evidence
    ENVIRONMENT "BREAKWATER_TEST_ARTIFACT_DIR=${_pipe_evidence_dir}")
  if(_breakwater_torch_probe EQUAL 0)
    foreach(_history_size normal compact)
      set(_history_args --import-session "${_pipe_evidence_dir}/scientific-history.json"
        --open-scientific-history --history-db "${_pipe_evidence_dir}/science-history-${_history_size}.sqlite" --smoke)
      if(_history_size STREQUAL "compact")
        list(APPEND _history_args --compact-ui)
      endif()
      add_test(NAME desktop.scientific_history_${_history_size} COMMAND breakwater ${_history_args})
      set_tests_properties(desktop.scientific_history_${_history_size} PROPERTIES FIXTURES_REQUIRED science_evidence TIMEOUT 30
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
    endforeach()
  endif()
  add_test(NAME desktop.pipe_evidence_smoke COMMAND breakwater
    --import-pipe-evidence "${_pipe_evidence_dir}/native-transport-evidence.json"
    --history-db "${_pipe_evidence_dir}/pipe-render.sqlite" --smoke)
  set_tests_properties(desktop.pipe_evidence_smoke PROPERTIES FIXTURES_REQUIRED pipe_evidence TIMEOUT 30)
  set_tests_properties(desktop.project desktop.qml_smoke desktop.pipe_evidence_smoke PROPERTIES
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
endif()

install(TARGETS breakwater RUNTIME DESTINATION bin)
install(FILES "${PROJECT_SOURCE_DIR}/.env.example" DESTINATION bin)
install(FILES "${PROJECT_SOURCE_DIR}/science/worker.py" DESTINATION share/breakwater/science)
install(FILES "${PROJECT_SOURCE_DIR}/science/worker_storage.py" "${PROJECT_SOURCE_DIR}/science/storage_grid.py"
  DESTINATION share/breakwater/science)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/science/breakwater_science" DESTINATION share/breakwater/science
  FILES_MATCHING PATTERN "*.py" PATTERN "__pycache__" EXCLUDE)
qt_generate_deploy_qml_app_script(TARGET breakwater OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
