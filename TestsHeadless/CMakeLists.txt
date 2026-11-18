cmake_minimum_required(VERSION 3.22)

project(mugen_headless LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT DEFINED AX_ROOT OR AX_ROOT STREQUAL "")
    if(DEFINED ENV{AX_ROOT} AND NOT "$ENV{AX_ROOT}" STREQUAL "")
        set(AX_ROOT "$ENV{AX_ROOT}")
    else()
        message(FATAL_ERROR "AX_ROOT is not set. Point it at the engine checkout that vendors doctest and rapidjson.")
    endif()
endif()
file(TO_CMAKE_PATH "${AX_ROOT}" AX_ROOT)

set(CLIENT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Source")
set(MUGEN_DIR "${CLIENT_SOURCE_DIR}/mugen")
set(DOCTEST_INCLUDE_DIR "${AX_ROOT}/3rdparty/doctest")
set(RAPIDJSON_INCLUDE_DIR "${AX_ROOT}/3rdparty")

if(NOT EXISTS "${DOCTEST_INCLUDE_DIR}/doctest.h")
    message(FATAL_ERROR "doctest.h not found at: ${DOCTEST_INCLUDE_DIR}")
endif()
if(NOT EXISTS "${RAPIDJSON_INCLUDE_DIR}/rapidjson/document.h")
    message(FATAL_ERROR "rapidjson not found at: ${RAPIDJSON_INCLUDE_DIR}")
endif()

function(mugen_collect_sources out_var)
    set(collected)
    foreach(pattern IN LISTS ARGN)
        file(GLOB_RECURSE matched CONFIGURE_DEPENDS ${pattern})
        list(APPEND collected ${matched})
    endforeach()
    set(${out_var} ${collected} PARENT_SCOPE)
endfunction()

mugen_collect_sources(MUGEN_CORE_SOURCES
    "${MUGEN_DIR}/core/*.h"
    "${MUGEN_DIR}/core/*.hpp"
    "${MUGEN_DIR}/core/*.cpp"
    "${MUGEN_DIR}/core/*.c"
)
mugen_collect_sources(MUGEN_CONF_SOURCES
    "${MUGEN_DIR}/conf/*.h"
    "${MUGEN_DIR}/conf/*.hpp"
    "${MUGEN_DIR}/conf/*.cpp"
    "${MUGEN_DIR}/conf/*.c"
)
mugen_collect_sources(MUGEN_AVATAR_SOURCES
    "${MUGEN_DIR}/avatar/*.h"
    "${MUGEN_DIR}/avatar/*.hpp"
    "${MUGEN_DIR}/avatar/*.cpp"
    "${MUGEN_DIR}/avatar/*.c"
)
mugen_collect_sources(MUGEN_SIM_SOURCES
    "${MUGEN_DIR}/sim/*.h"
    "${MUGEN_DIR}/sim/*.cpp"
)
# 效果摘要格式（附录 A）：解析、比对，不依赖 sim 实现。
mugen_collect_sources(SUMMARY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/summary/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/summary/*.cpp"
)
list(FILTER SUMMARY_SOURCES EXCLUDE REGEX ".*/summary/tests/.*")
mugen_collect_sources(SUMMARY_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/summary/tests/*.cpp"
)

set(MUGEN_BASE_SOURCES
    ${MUGEN_CORE_SOURCES}
    ${MUGEN_CONF_SOURCES}
    ${MUGEN_AVATAR_SOURCES}
)

# Rendering stays on the client.
list(FILTER MUGEN_BASE_SOURCES EXCLUDE REGEX ".*/avatar/render/.*")

include("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/VfsSources.cmake")
list(APPEND MUGEN_BASE_SOURCES ${VFS_SOURCES})

set(MUGEN_COMMON_INCLUDES
    "${CLIENT_SOURCE_DIR}"
    "${CLIENT_SOURCE_DIR}/3rd"
    "${RAPIDJSON_INCLUDE_DIR}"
)

function(mugen_apply_compile_settings target_name)
    target_compile_definitions(${target_name} PRIVATE RUNTIME_IN_AXMOL=0)
    target_include_directories(${target_name} PRIVATE ${MUGEN_COMMON_INCLUDES})
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /utf-8 /permissive- /W4 /bigobj)
        # Headless logs use std::format. The engine path keeps fmt via axmol.
        target_compile_options(${target_name} PRIVATE /Zc:preprocessor)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra)
    endif()
endfunction()

mugen_collect_sources(MUGEN_SIM_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/sim_tests/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/sim_tests/*.cpp"
)
mugen_collect_sources(SIM_CLI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/sim_cli/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/sim_cli/*.cpp"
)

# 公共部分只编译一次。用 OBJECT 库而不是静态库，避免链接器丢弃只靠静态初始化注册的目标文件。
add_library(mugen_base OBJECT ${MUGEN_BASE_SOURCES} ${SUMMARY_SOURCES})
mugen_apply_compile_settings(mugen_base)
add_library(mugen_sim OBJECT ${MUGEN_SIM_SOURCES})
mugen_apply_compile_settings(mugen_sim)

add_executable(mugen_sim_tests
    $<TARGET_OBJECTS:mugen_base>
    $<TARGET_OBJECTS:mugen_sim>
    ${MUGEN_SIM_TEST_SOURCES}
    ${SUMMARY_TEST_SOURCES}
    "${DOCTEST_INCLUDE_DIR}/doctest.cpp"
)
target_include_directories(mugen_sim_tests PRIVATE "${DOCTEST_INCLUDE_DIR}")
mugen_apply_compile_settings(mugen_sim_tests)

add_executable(mugen_sim_cli
    $<TARGET_OBJECTS:mugen_base>
    $<TARGET_OBJECTS:mugen_sim>
    ${SIM_CLI_SOURCES}
)
mugen_apply_compile_settings(mugen_sim_cli)

set(MUGEN_HEADLESS_TARGETS mugen_sim_tests mugen_sim_cli)

find_package(Threads REQUIRED)
foreach(target IN LISTS MUGEN_HEADLESS_TARGETS)
    target_compile_definitions(${target} PRIVATE MUGEN_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../Content")
    target_link_libraries(${target} PRIVATE Threads::Threads)
endforeach()
