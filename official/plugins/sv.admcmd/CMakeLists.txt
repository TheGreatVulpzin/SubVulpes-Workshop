# Builds sv.admcmd with the SDK. Pass -DSV_SDK_ROOT=<the extracted SDK folder>. tools/build_plugin.ps1 does it.
cmake_minimum_required(VERSION 3.20)
project(sv_admcmd C)
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Use MSVC x86 for this plugin.")
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(SV_SDK_ROOT "" CACHE PATH "The extracted SDK folder")
if(NOT EXISTS "${SV_SDK_ROOT}/tools/sv_plugin.cmake")
    message(FATAL_ERROR "Set SV_SDK_ROOT to the SDK folder.")
endif()
include("${SV_SDK_ROOT}/tools/sv_plugin.cmake")

set(COMMON "${CMAKE_CURRENT_SOURCE_DIR}/../../common")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/plugin.ini" version_line REGEX "^version=")
string(REGEX REPLACE "^version=" "" PLUGIN_VERSION "${version_line}")
sv_add_plugin(sv-admcmd ID sv.admcmd MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.ini"
    SOURCES admcmd.c "${COMMON}/official.c" "${COMMON}/permissions.c" OFFICIAL)

# Tests: a plugin that probes the commands, and a native test.
sv_add_plugin(sv-admcmd-probe ID test.admcmd MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/tests/admcmd_probe/plugin.ini"
    SOURCES tests/admcmd_probe.c "${COMMON}/official.c")

# The native test includes the source of the plugin, so it can reach its static functions.
add_executable(admcmd-unit-test tests/admcmd_unit_test.c "${COMMON}/official.c" "${COMMON}/permissions.c")
target_include_directories(admcmd-unit-test PRIVATE "${SV_PLUGIN_INCLUDE_DIR}")
target_compile_definitions(admcmd-unit-test PRIVATE _CRT_SECURE_NO_WARNINGS
    SV_OFFICIAL_ID="sv.admcmd" SV_OFFICIAL_VERSION="${PLUGIN_VERSION}")
target_compile_options(admcmd-unit-test PRIVATE /W4 /WX /utf-8)
