qt_standard_project_setup(REQUIRES 6.12)

qt_add_library(breakwater_project_document STATIC ProjectDocument.cpp ProjectDocument.hpp
  quality/PipeParameters.hpp quality/PipeParameters.cpp)
target_include_directories(breakwater_project_document PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(breakwater_project_document PUBLIC breakwater_engine Qt6::Gui)

qt_add_library(breakwater_telemetry STATIC
  telemetry/TelemetryContracts.hpp telemetry/TelemetryContracts.cpp
  telemetry/SensorProject.hpp telemetry/SensorProject.cpp
  "${PROJECT_SOURCE_DIR}/contracts/include/breakwater/telemetry.hpp")
target_include_directories(breakwater_telemetry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(breakwater_telemetry PUBLIC breakwater_project_document Qt6::Core)

qt_add_library(breakwater_desktop_base STATIC ProjectController.cpp ProjectController.hpp)
target_include_directories(breakwater_desktop_base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(breakwater_desktop_base PUBLIC breakwater_telemetry Qt6::Quick Qt6::Concurrent)

qt_add_library(breakwater_telemetry_runtime STATIC
  telemetry/TelemetryStream.hpp telemetry/TelemetryStream.cpp
  telemetry/TelemetryController.hpp telemetry/TelemetryController.cpp)
target_include_directories(breakwater_telemetry_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(breakwater_telemetry_runtime PUBLIC breakwater_telemetry Qt6::Core)

qt_add_library(breakwater_eog_contracts STATIC eog/EogContracts.hpp eog/EogContracts.cpp)
target_include_directories(breakwater_eog_contracts PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(breakwater_eog_contracts PUBLIC breakwater_telemetry)
qt_add_resources(breakwater_eog_contracts eog_contract_schemas
  PREFIX /breakwater/eog/contracts BASE "${PROJECT_SOURCE_DIR}/contracts/eog"
  FILES
    "${PROJECT_SOURCE_DIR}/contracts/eog/common-v1.schema.json"
    "${PROJECT_SOURCE_DIR}/contracts/eog/capture-manifest-v1.schema.json"
    "${PROJECT_SOURCE_DIR}/contracts/eog/analysis-request-v1.schema.json"
    "${PROJECT_SOURCE_DIR}/contracts/eog/analysis-response-v1.schema.json")

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
target_link_libraries(breakwater PRIVATE breakwater_agent breakwater_telemetry_runtime breakwater_eog_workflow Qt6::QuickControls2 Qt6::Quick3D)
get_filename_component(_qt_prefix "${Qt6_DIR}/../../.." ABSOLUTE)
set_target_properties(breakwater PROPERTIES VS_DEBUGGER_ENVIRONMENT "PATH=${_qt_prefix}/bin;%PATH%")
qt_add_qml_module(breakwater URI Breakwater VERSION 1.0
  QML_FILES qml/Main.qml qml/Equipment.qml qml/PipeRoute.qml qml/AgentPanel.qml qml/TrajectoryChart.qml qml/PipeTransportPanel.qml qml/StorageSciencePanel.qml
    qml/SensorPanel.qml qml/TelemetryPanel.qml qml/SensorTraceChart.qml
    qml/EogDatasetPanel.qml qml/EogNetworkView.qml qml/EogTrendChart.qml
    qml/EogAnalysisPanel.qml qml/EogPlanChart.qml qml/EogPlanDetails.qml qml/EogEventDetails.qml)

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
  foreach(_suite sensor_editor telemetry_stream telemetry_lifecycle)
    qt_add_executable(breakwater_${_suite}_tests tests/${_suite}_tests.cpp)
    target_link_libraries(breakwater_${_suite}_tests PRIVATE breakwater_desktop_base breakwater_telemetry_runtime Qt6::Test)
    target_compile_definitions(breakwater_${_suite}_tests PRIVATE BREAKWATER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    if(Python3_Interpreter_FOUND)
      file(TO_CMAKE_PATH "${Python3_EXECUTABLE}" _g2_test_python)
      target_compile_definitions(breakwater_${_suite}_tests PRIVATE BREAKWATER_TEST_PYTHON="${_g2_test_python}")
    endif()
    add_test(NAME desktop.${_suite} COMMAND breakwater_${_suite}_tests
      -o "${CMAKE_CURRENT_BINARY_DIR}/${_suite}-results.txt,txt")
    set_tests_properties(desktop.${_suite} PROPERTIES TIMEOUT 180
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  endforeach()
  # This aggregate functional suite processes 33,024 readings and 32,768
  # heartbeats. Local ASan took 174.72 s; hosted execution exceeded 180 s.
  # Its harness budget is separate from the unchanged stream/command deadlines.
  if(BREAKWATER_ENABLE_ASAN)
    set_tests_properties(desktop.telemetry_stream PROPERTIES TIMEOUT 360)
  endif()
  set(_g2_evidence_dir "${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/g2")
  target_link_libraries(breakwater_sensor_editor_tests PRIVATE breakwater_agent)
  set_tests_properties(desktop.telemetry_lifecycle PROPERTIES FIXTURES_SETUP g2_recording
    ENVIRONMENT "BREAKWATER_TEST_ARTIFACT_DIR=${_g2_evidence_dir}")
  foreach(_g2_size normal compact)
    set(_g2_sensor_args --sensor-editor-check --smoke --screenshot "${CMAKE_CURRENT_BINARY_DIR}/g2-sensors-${_g2_size}.png")
    set(_g2_replay_args --project "${_g2_evidence_dir}/g2-monitoring-project.json"
      --telemetry-replay "${_g2_evidence_dir}/g2-recording.ndjson" --smoke
      --screenshot "${CMAKE_CURRENT_BINARY_DIR}/g2-replay-${_g2_size}.png")
    if(_g2_size STREQUAL "compact")
      list(APPEND _g2_sensor_args --compact-ui)
      list(APPEND _g2_replay_args --compact-ui)
    endif()
    add_test(NAME desktop.sensor_editor_${_g2_size}_smoke COMMAND breakwater ${_g2_sensor_args})
    add_test(NAME desktop.telemetry_replay_${_g2_size}_smoke COMMAND breakwater ${_g2_replay_args})
    set_tests_properties(desktop.sensor_editor_${_g2_size}_smoke desktop.telemetry_replay_${_g2_size}_smoke
      PROPERTIES TIMEOUT 30 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
    set_tests_properties(desktop.telemetry_replay_${_g2_size}_smoke PROPERTIES FIXTURES_REQUIRED g2_recording)
  endforeach()
  qt_add_executable(breakwater_telemetry_contract_tests tests/telemetry_contract_tests.cpp)
  target_link_libraries(breakwater_telemetry_contract_tests PRIVATE breakwater_telemetry Qt6::Test)
  target_compile_definitions(breakwater_telemetry_contract_tests PRIVATE BREAKWATER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  add_test(NAME desktop.telemetry_contracts COMMAND breakwater_telemetry_contract_tests
    -o "${CMAKE_CURRENT_BINARY_DIR}/telemetry_contracts-results.txt,txt")
  set_tests_properties(desktop.telemetry_contracts PROPERTIES TIMEOUT 60
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  qt_add_executable(breakwater_telemetry_contract_check tests/telemetry_contract_check.cpp)
  target_link_libraries(breakwater_telemetry_contract_check PRIVATE breakwater_telemetry Qt6::Core)
  if(Python3_Interpreter_FOUND)
    add_test(NAME contracts.telemetry COMMAND "${Python3_EXECUTABLE}" -B
      "${PROJECT_SOURCE_DIR}/contracts/telemetry/tests/test_contracts.py"
      --native "$<TARGET_FILE:breakwater_telemetry_contract_check>")
    set_tests_properties(contracts.telemetry PROPERTIES TIMEOUT 120
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  endif()
  qt_add_executable(breakwater_eog_contract_tests tests/eog_contract_tests.cpp)
  target_link_libraries(breakwater_eog_contract_tests PRIVATE breakwater_eog_contracts Qt6::Test)
  target_compile_definitions(breakwater_eog_contract_tests PRIVATE BREAKWATER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
  add_test(NAME desktop.eog_contracts COMMAND breakwater_eog_contract_tests
    -o "${CMAKE_CURRENT_BINARY_DIR}/eog_contracts-results.txt,txt")
  set_tests_properties(desktop.eog_contracts PROPERTIES TIMEOUT 60
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  if(Python3_Interpreter_FOUND)
    add_test(NAME contracts.eog COMMAND "${Python3_EXECUTABLE}" -B
      "${PROJECT_SOURCE_DIR}/contracts/eog/tests/test_contracts.py"
      --native "$<TARGET_FILE:breakwater_eog_contract_tests>")
    set_tests_properties(contracts.eog PROPERTIES TIMEOUT 120
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  endif()
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

# Read-only challenge input and presentation stay separate from the simulator.
qt_add_library(breakwater_eog STATIC
  eog/EogDataset.hpp eog/EogDataset.cpp eog/EogDatasetController.hpp eog/EogDatasetController.cpp
  eog/EogCapture.hpp eog/EogCapture.cpp eog/EogAnalysisClient.hpp eog/EogAnalysisClient.cpp
  eog/EogHistoryRepository.hpp eog/EogHistoryRepository.cpp)
target_include_directories(breakwater_eog PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(breakwater_eog PUBLIC breakwater_eog_contracts Qt6::Concurrent Qt6::Sql)
if(WIN32)
  target_link_libraries(breakwater_eog PRIVATE Cabinet)
endif()
target_compile_definitions(breakwater_eog PRIVATE
  BREAKWATER_EOG_WORKER_PATH="${PROJECT_SOURCE_DIR}/science/eog_worker.py"
  BREAKWATER_EOG_CONTRACTS_PATH="${PROJECT_SOURCE_DIR}/contracts/eog"
  BREAKWATER_EOG_NATIVE_LIBRARY_PATH="$<TARGET_FILE:breakwater_eog_response>"
  BREAKWATER_EOG_NATIVE_LIBRARY_NAME="$<TARGET_FILE_NAME:breakwater_eog_response>"
  BREAKWATER_DEVELOPMENT_BIN_DIR="$<TARGET_FILE_DIR:breakwater>")
add_dependencies(breakwater breakwater_eog_response)
# Python's restricted Windows DLL search needs the checked runtime beside the
# provider DLL. The native target belongs to engine/, so stage it through a
# dependency rather than attaching a cross-directory TARGET POST_BUILD command.
if(MSVC AND BREAKWATER_ENABLE_ASAN)
  get_filename_component(_eog_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(BREAKWATER_EOG_ASAN_RUNTIME
    "${_eog_compiler_bin}/clang_rt.asan_dynamic-x86_64.dll" ABSOLUTE)
  if(NOT EXISTS "${BREAKWATER_EOG_ASAN_RUNTIME}")
    message(FATAL_ERROR "The matching compiler's EOG AddressSanitizer runtime is unavailable: ${BREAKWATER_EOG_ASAN_RUNTIME}")
  endif()
  add_custom_target(breakwater_eog_provider_runtime
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${BREAKWATER_EOG_ASAN_RUNTIME}"
      "$<TARGET_FILE_DIR:breakwater_eog_response>/clang_rt.asan_dynamic-x86_64.dll"
    VERBATIM)
  add_dependencies(breakwater_eog_provider_runtime breakwater_eog_response)
  add_dependencies(breakwater breakwater_eog_provider_runtime)
endif()
qt_add_library(breakwater_eog_workflow STATIC
  eog/EogInvestigator.hpp eog/EogInvestigator.cpp
  eog/EogAnalysisController.hpp eog/EogAnalysisController.cpp)
target_include_directories(breakwater_eog_workflow PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(breakwater_eog_workflow PUBLIC breakwater_eog breakwater_agent)
qt_add_executable(breakwater_eog_live_check eog/live_check.cpp)
target_link_libraries(breakwater_eog_live_check PRIVATE breakwater_eog_workflow)
add_dependencies(breakwater_eog_live_check breakwater_eog_response)
if(TARGET breakwater_eog_provider_runtime)
  add_dependencies(breakwater_eog_live_check breakwater_eog_provider_runtime)
endif()
if(BUILD_TESTING)
  qt_add_executable(breakwater_eog_dataset_tests tests/eog_dataset_tests.cpp)
  target_link_libraries(breakwater_eog_dataset_tests PRIVATE breakwater_eog Qt6::Test)
  add_test(NAME desktop.eog_dataset COMMAND breakwater_eog_dataset_tests
    -o "${CMAKE_CURRENT_BINARY_DIR}/eog_dataset-results.txt,txt")
  set_tests_properties(desktop.eog_dataset PROPERTIES TIMEOUT 180
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  qt_add_executable(breakwater_eog_capture_tests tests/eog_capture_tests.cpp)
  target_link_libraries(breakwater_eog_capture_tests PRIVATE breakwater_eog Qt6::Test)
  add_test(NAME desktop.eog_capture COMMAND breakwater_eog_capture_tests
    -o "${CMAKE_CURRENT_BINARY_DIR}/eog_capture-results.txt,txt")
  set_tests_properties(desktop.eog_capture PROPERTIES TIMEOUT 180
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  foreach(_eog_suite analysis_client history)
    qt_add_executable(breakwater_eog_${_eog_suite}_tests tests/eog_${_eog_suite}_tests.cpp)
    target_link_libraries(breakwater_eog_${_eog_suite}_tests PRIVATE breakwater_eog Qt6::Test)
    target_compile_definitions(breakwater_eog_${_eog_suite}_tests PRIVATE BREAKWATER_TEST_PYTHON="${_breakwater_test_python}")
    add_test(NAME desktop.eog_${_eog_suite} COMMAND breakwater_eog_${_eog_suite}_tests
      -o "${CMAKE_CURRENT_BINARY_DIR}/eog_${_eog_suite}-results.txt,txt")
    set_tests_properties(desktop.eog_${_eog_suite} PROPERTIES TIMEOUT 240
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  endforeach()
  foreach(_eog_workflow_suite investigator analysis_controller)
    qt_add_executable(breakwater_eog_${_eog_workflow_suite}_tests tests/eog_${_eog_workflow_suite}_tests.cpp)
    target_link_libraries(breakwater_eog_${_eog_workflow_suite}_tests PRIVATE breakwater_eog_workflow Qt6::Test)
    target_compile_definitions(breakwater_eog_${_eog_workflow_suite}_tests PRIVATE BREAKWATER_TEST_PYTHON="${_breakwater_test_python}")
    add_test(NAME desktop.eog_${_eog_workflow_suite} COMMAND breakwater_eog_${_eog_workflow_suite}_tests
      -o "${CMAKE_CURRENT_BINARY_DIR}/eog_${_eog_workflow_suite}-results.txt,txt")
    set_tests_properties(desktop.eog_${_eog_workflow_suite} PROPERTIES TIMEOUT 240
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  endforeach()
  qt_add_executable(breakwater_eog_provider_tests tests/eog_provider_integration_tests.cpp)
  target_link_libraries(breakwater_eog_provider_tests PRIVATE breakwater_eog_workflow Qt6::Test)
  target_compile_definitions(breakwater_eog_provider_tests PRIVATE
    BREAKWATER_TEST_PYTHON="${_breakwater_test_python}"
    BREAKWATER_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    BREAKWATER_EOG_WORKER_PATH="${PROJECT_SOURCE_DIR}/science/eog_worker.py"
    BREAKWATER_TEST_EOG_NATIVE_LIBRARY="$<TARGET_FILE:breakwater_eog_response>")
  add_dependencies(breakwater_eog_provider_tests breakwater_eog_response)
  if(TARGET breakwater_eog_provider_runtime)
    add_dependencies(breakwater_eog_provider_tests breakwater_eog_provider_runtime)
  endif()
  add_test(NAME desktop.eog_provider COMMAND breakwater_eog_provider_tests
    -o "${CMAKE_CURRENT_BINARY_DIR}/eog_provider-results.txt,txt")
  set_tests_properties(desktop.eog_provider PROPERTIES TIMEOUT 180
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  if(Python3_Interpreter_FOUND)
    add_test(NAME desktop.eog_demo_input COMMAND "${Python3_EXECUTABLE}" -B
      "${PROJECT_SOURCE_DIR}/scripts/tests/test_eog_demo.py")
    set_tests_properties(desktop.eog_demo_input PROPERTIES TIMEOUT 120)
    add_test(NAME desktop.eog_live_cli COMMAND "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/eog_live_cli_tests.py" "$<TARGET_FILE:breakwater_eog_live_check>"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/eog_fixtures/render")
    set_tests_properties(desktop.eog_live_cli PROPERTIES TIMEOUT 120
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
    add_test(NAME desktop.eog_cli COMMAND "${Python3_EXECUTABLE}" -B
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/eog_cli_tests.py" "$<TARGET_FILE:breakwater>"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/eog_fixtures/render")
    set_tests_properties(desktop.eog_cli PROPERTIES TIMEOUT 180
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  endif()
  foreach(_eog_view plan_chart plan_details event_details)
    qt_add_executable(breakwater_eog_${_eog_view}_tests tests/eog_${_eog_view}_tests.cpp)
    target_link_libraries(breakwater_eog_${_eog_view}_tests PRIVATE breakwater_options Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    target_compile_definitions(breakwater_eog_${_eog_view}_tests PRIVATE BREAKWATER_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    add_test(NAME desktop.eog_${_eog_view} COMMAND breakwater_eog_${_eog_view}_tests
      -o "${CMAKE_CURRENT_BINARY_DIR}/eog_${_eog_view}-results.txt,txt")
    set_tests_properties(desktop.eog_${_eog_view} PROPERTIES TIMEOUT 120
      ENVIRONMENT "QSG_RHI_BACKEND=d3d11;QSG_RHI_PREFER_SOFTWARE_RENDERER=1"
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
  endforeach()
  foreach(_eog_size normal compact)
    set(_eog_analysis_args --workspace eog --eog-dataset "${CMAKE_CURRENT_SOURCE_DIR}/tests/eog_fixtures/render"
      --eog-analysis-panel --smoke --eog-report "${CMAKE_CURRENT_BINARY_DIR}/eog-analysis-${_eog_size}.json"
      --screenshot "${CMAKE_CURRENT_BINARY_DIR}/eog-analysis-${_eog_size}.png"
      --history-db "${CMAKE_CURRENT_BINARY_DIR}/eog-analysis-${_eog_size}.sqlite")
    if(_eog_size STREQUAL "compact")
      list(APPEND _eog_analysis_args --compact-ui)
    endif()
    add_test(NAME desktop.eog_analysis_${_eog_size} COMMAND breakwater ${_eog_analysis_args})
    set_tests_properties(desktop.eog_analysis_${_eog_size} PROPERTIES TIMEOUT 45
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
    foreach(_eog_scene overview facility)
      set(_eog_render_args --workspace eog --eog-dataset "${CMAKE_CURRENT_SOURCE_DIR}/tests/eog_fixtures/render"
        --smoke --eog-report "${CMAKE_CURRENT_BINARY_DIR}/eog-${_eog_scene}-${_eog_size}.json"
        --screenshot "${CMAKE_CURRENT_BINARY_DIR}/eog-${_eog_scene}-${_eog_size}.png"
        --history-db "${CMAKE_CURRENT_BINARY_DIR}/eog-${_eog_scene}-${_eog_size}.sqlite")
      if(_eog_size STREQUAL "compact")
        list(APPEND _eog_render_args --compact-ui)
      endif()
      if(_eog_scene STREQUAL "facility")
        list(APPEND _eog_render_args --eog-facility facility-Z)
      endif()
      add_test(NAME desktop.eog_${_eog_scene}_${_eog_size} COMMAND breakwater ${_eog_render_args})
      set_tests_properties(desktop.eog_${_eog_scene}_${_eog_size} PROPERTIES TIMEOUT 45
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${_qt_prefix}/bin")
    endforeach()
  endforeach()
endif()
include(eog/InstallProvider.cmake)
