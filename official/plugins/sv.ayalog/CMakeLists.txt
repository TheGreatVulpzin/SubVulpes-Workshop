# Builds sv.ayalog with the SDK. Pass -DSV_SDK_ROOT=<the extracted SDK folder>. tools/build_plugin.ps1 does it.
cmake_minimum_required(VERSION 3.20)
project(sv_ayalog C)
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
sv_add_plugin(sv-ayalog ID sv.ayalog MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.ini"
    SOURCES ayalog.c "${COMMON}/official.c" "${COMMON}/permissions.c" OFFICIAL)

# The native test includes the source of the plugin, so it can reach its static functions.
add_executable(ayalog-unit-test tests/ayalog_unit_test.c "${COMMON}/official.c" "${COMMON}/permissions.c")
target_include_directories(ayalog-unit-test PRIVATE "${SV_PLUGIN_INCLUDE_DIR}")
target_compile_definitions(ayalog-unit-test PRIVATE _CRT_SECURE_NO_WARNINGS
    SV_OFFICIAL_ID="sv.ayalog" SV_OFFICIAL_VERSION="${PLUGIN_VERSION}")
target_compile_options(ayalog-unit-test PRIVATE /W4 /WX /utf-8)
