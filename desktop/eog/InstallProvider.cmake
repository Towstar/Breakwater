# Install only the production capture provider and its exact local contract.
# The offline inspection/qualification CLIs are intentionally outside the app.
if(TARGET breakwater_eog_response)
  install(TARGETS breakwater_eog_response breakwater_eog_live_check
    RUNTIME DESTINATION bin LIBRARY DESTINATION lib)
  if(MSVC AND BREAKWATER_ENABLE_ASAN)
    if(NOT EXISTS "${BREAKWATER_EOG_ASAN_RUNTIME}")
      message(FATAL_ERROR "The checked EOG provider requires its matching staged ASan runtime.")
    endif()
    install(FILES "${BREAKWATER_EOG_ASAN_RUNTIME}" DESTINATION bin)
  endif()
  install(FILES
    "${PROJECT_SOURCE_DIR}/science/eog_worker.py"
    "${PROJECT_SOURCE_DIR}/science/eog_detection.py"
    "${PROJECT_SOURCE_DIR}/science/eog_inputs.py"
    "${PROJECT_SOURCE_DIR}/science/eog_model.py"
    "${PROJECT_SOURCE_DIR}/science/eog_observations.py"
    "${PROJECT_SOURCE_DIR}/science/eog_response.py"
    "${PROJECT_SOURCE_DIR}/science/eog_plan_validator.py"
    "${PROJECT_SOURCE_DIR}/science/native_eog.py"
    DESTINATION share/breakwater/science)
  install(FILES
    "${PROJECT_SOURCE_DIR}/contracts/eog/validate.py"
    "${PROJECT_SOURCE_DIR}/contracts/eog/common-v1.schema.json"
    "${PROJECT_SOURCE_DIR}/contracts/eog/analysis-request-v1.schema.json"
    "${PROJECT_SOURCE_DIR}/contracts/eog/analysis-response-v1.schema.json"
    "${PROJECT_SOURCE_DIR}/contracts/eog/capture-manifest-v1.schema.json"
    DESTINATION share/breakwater/contracts/eog)
endif()
