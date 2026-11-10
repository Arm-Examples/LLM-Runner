#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

find_package(Threads REQUIRED)

add_library(rebar_ml_core SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/nodes/tee_node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/actor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/node_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/node_session_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/runtime/pipeline_session_impl.cpp)

set_target_properties(rebar_ml_core PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
llm_configure_rpath(rebar_ml_core)

target_include_directories(rebar_ml_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(rebar_ml_core PUBLIC
    Threads::Threads)

if(USE_CF)
    target_link_libraries(rebar_ml_core PRIVATE rebar_ml_compute_flow)
endif()

if (BUILD_JNI_LIB AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/core/jni")
    include(find-jni)

    add_library(rebar_ml_jni_core SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/jni/rebar_jni_conversions.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/jni/tee_jni.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core/jni/rebar_jni_core.cpp)

    set_target_properties(rebar_ml_jni_core PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
    llm_configure_rpath(rebar_ml_jni_core)

    target_link_libraries(rebar_ml_jni_core PUBLIC
        rebar_ml_core)

    target_include_directories(rebar_ml_jni_core PRIVATE
        ${JNI_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()
