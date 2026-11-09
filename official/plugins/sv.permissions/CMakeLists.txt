# Builds sv.permissions with the SDK. Pass -DSV_SDK_ROOT=<the extracted SDK folder>. tools/build_plugin.ps1 does it.
cmake_minimum_required(VERSION 3.20)
project(sv_permissions C)
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
sv_add_plugin(sv-permissions ID sv.permissions MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.ini"
    SOURCES permissions.c groups.c "${COMMON}/official.c" OFFICIAL)

# Tests: a plugin that uses the service, and a native test of the group rules.
sv_add_plugin(sv-permission-probe ID test.permissions
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/tests/permission_probe/plugin.ini"
    SOURCES tests/permission_probe.c "${COMMON}/official.c")
add_executable(permission-groups-test tests/permission_groups_test.c groups.c "${COMMON}/official.c")
target_include_directories(permission-groups-test PRIVATE "${SV_PLUGIN_INCLUDE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(permission-groups-test PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(permission-groups-test PRIVATE /W4 /WX /utf-8)
