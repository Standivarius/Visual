cmake_minimum_required(VERSION 3.20)
project(VisualApp LANGUAGES CXX RC)

enable_testing()

include(${CMAKE_CURRENT_SOURCE_DIR}/version.cmake)
if(NOT DEFINED VISUAL_VERSION_SEMVER OR VISUAL_VERSION_SEMVER STREQUAL "")
    set(VISUAL_VERSION_SEMVER "${VISUAL_DEFAULT_VERSION}")
endif()

string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)([-+].*)?$" VISUAL_VERSION_MATCH "${VISUAL_VERSION_SEMVER}")
if(NOT VISUAL_VERSION_MATCH)
    message(FATAL_ERROR "VISUAL_VERSION_SEMVER must begin with MAJOR.MINOR.PATCH and may include a SemVer suffix: ${VISUAL_VERSION_SEMVER}")
endif()
set(VISUAL_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(VISUAL_VERSION_MINOR "${CMAKE_MATCH_2}")
set(VISUAL_VERSION_PATCH "${CMAKE_MATCH_3}")
if(VISUAL_VERSION_SEMVER MATCHES "-")
    set(VISUAL_VERSION_FILEFLAGS "VS_FF_PRERELEASE")
else()
    set(VISUAL_VERSION_FILEFLAGS "0x0L")
endif()

set(VISUAL_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${VISUAL_GENERATED_DIR})
configure_file(src/version.h.in ${VISUAL_GENERATED_DIR}/visual_version.h @ONLY)
configure_file(src/version.rc.in ${VISUAL_GENERATED_DIR}/visual_version.rc @ONLY)

add_library(visual_core INTERFACE)
target_include_directories(visual_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${VISUAL_GENERATED_DIR}
)
target_compile_features(visual_core INTERFACE cxx_std_20)

add_executable(visual_app WIN32
    src/main.cpp
    app.manifest
    ${VISUAL_GENERATED_DIR}/visual_version.rc
)
target_link_libraries(visual_app PRIVATE
    visual_core
    d3d11
    dxgi
    d3dcompiler
    windowsapp
    shell32
    user32
    gdi32
    ole32
    oleaut32
    uiautomationcore
)
target_compile_definitions(visual_app PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(visual_app PRIVATE /W4 /permissive- /EHsc)

add_executable(visual_diagnostics
    src/diagnostics_main.cpp
    ${VISUAL_GENERATED_DIR}/visual_version.rc
)
target_link_libraries(visual_diagnostics PRIVATE dxgi shcore user32)
target_include_directories(visual_diagnostics PRIVATE ${VISUAL_GENERATED_DIR})
target_compile_features(visual_diagnostics PRIVATE cxx_std_20)
target_compile_definitions(visual_diagnostics PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(visual_diagnostics PRIVATE /W4 /permissive- /EHsc)

add_executable(visual_core_pipeline_tests tests/core_pipeline_tests.cpp)
target_link_libraries(visual_core_pipeline_tests PRIVATE visual_core)
target_compile_options(visual_core_pipeline_tests PRIVATE /W4 /permissive- /EHsc)
add_test(NAME core_pipeline_tests COMMAND visual_core_pipeline_tests)

add_executable(visual_locator_model_tests tests/locator_model_tests.cpp)
target_link_libraries(visual_locator_model_tests PRIVATE visual_core)
target_compile_options(visual_locator_model_tests PRIVATE /W4 /permissive- /EHsc)
add_test(NAME locator_model_tests COMMAND visual_locator_model_tests)

add_executable(visual_poi_arbitration_tests tests/poi_arbitration_tests.cpp)
target_link_libraries(visual_poi_arbitration_tests PRIVATE visual_core)
target_compile_options(visual_poi_arbitration_tests PRIVATE /W4 /permissive- /EHsc)
add_test(NAME poi_arbitration_tests COMMAND visual_poi_arbitration_tests)

add_executable(visual_view_return_tests tests/view_return_tests.cpp)
target_link_libraries(visual_view_return_tests PRIVATE visual_core)
target_compile_options(visual_view_return_tests PRIVATE /W4 /permissive- /EHsc)
add_test(NAME view_return_tests COMMAND visual_view_return_tests)

add_test(NAME diagnostics_json COMMAND visual_diagnostics)
set_tests_properties(diagnostics_json PROPERTIES PASS_REGULAR_EXPRESSION "\\\"schema_version\\\": \\\"1\\\"")

set_property(TARGET visual_app PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Visual semantic version: ${VISUAL_VERSION_SEMVER}")
message(STATUS "Visual package id/channel: ${VISUAL_PACKAGE_ID} / ${VISUAL_RELEASE_CHANNEL}")
