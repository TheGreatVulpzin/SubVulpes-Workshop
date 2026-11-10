# Builds sv.bans with the SDK. Pass -DSV_SDK_ROOT=<the extracted SDK folder>. tools/build_plugin.ps1 does it.
cmake_minimum_required(VERSION 3.20)
project(sv_bans C)
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
sv_add_plugin(sv-bans ID sv.bans MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/plugin.ini"
    SOURCES bans.c bans_table.c "${COMMON}/official.c" "${COMMON}/permissions.c" OFFICIAL)

# The table has no host calls, so a native test runs it alone.
add_executable(bans-table-test tests/bans_table_test.c bans_table.c)
target_compile_definitions(bans-table-test PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(bans-table-test PRIVATE /W4 /WX /utf-8)
