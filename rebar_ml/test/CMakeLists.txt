#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

message(STATUS "Adding Rebar C++ tests")

include(find-catch)

set(REBAR_ROOT_BIN_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
set(REBAR_ROOT_LIB_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

set(REBAR_CORE_CPP_TEST_TARGET rebar_ml_core_cpp_tests)
add_executable(${REBAR_CORE_CPP_TEST_TARGET}
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/RebarTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp/RebarConnectionOptionsTest.cpp)
llm_configure_rpath(${REBAR_CORE_CPP_TEST_TARGET})
target_include_directories(${REBAR_CORE_CPP_TEST_TARGET} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/cpp/src)
target_compile_definitions(${REBAR_CORE_CPP_TEST_TARGET} PRIVATE
    REBAR_ROOT_BIN_DIR="${REBAR_ROOT_BIN_DIR}"
    REBAR_ROOT_LIB_DIR="${REBAR_ROOT_LIB_DIR}")
target_link_libraries(${REBAR_CORE_CPP_TEST_TARGET} PUBLIC
    rebar_ml_core
    Catch2::Catch2WithMain)

add_test(NAME rebar_ml_core_cpp_ctest COMMAND ${REBAR_CORE_CPP_TEST_TARGET})
set_tests_properties(rebar_ml_core_cpp_ctest PROPERTIES
    ENVIRONMENT
    "REBAR_ROOT_BIN_DIR=${REBAR_ROOT_BIN_DIR};REBAR_ROOT_LIB_DIR=${REBAR_ROOT_LIB_DIR}")

if(TARGET rebar_ml_model_wavtopcm AND TARGET rebar_ml_model_pcms16letof32le)
    set(REBAR_MODELS_CPP_TEST_TARGET rebar_ml_models_cpp_tests)
    add_executable(${REBAR_MODELS_CPP_TEST_TARGET}
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/RebarAudioUtilsTest.cpp)
    llm_configure_rpath(${REBAR_MODELS_CPP_TEST_TARGET})
    target_include_directories(${REBAR_MODELS_CPP_TEST_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/cpp/src)
    target_compile_definitions(${REBAR_MODELS_CPP_TEST_TARGET} PRIVATE
        REBAR_ROOT_BIN_DIR="${REBAR_ROOT_BIN_DIR}"
        REBAR_ROOT_LIB_DIR="${REBAR_ROOT_LIB_DIR}")
    target_link_libraries(${REBAR_MODELS_CPP_TEST_TARGET} PUBLIC
        rebar_ml_core
        rebar_ml_model_wavtopcm
        rebar_ml_model_pcms16letof32le
        Catch2::Catch2WithMain)

    add_test(NAME rebar_ml_models_cpp_ctest COMMAND ${REBAR_MODELS_CPP_TEST_TARGET})
    set_tests_properties(rebar_ml_models_cpp_ctest PROPERTIES
        ENVIRONMENT
        "REBAR_ROOT_BIN_DIR=${REBAR_ROOT_BIN_DIR};REBAR_ROOT_LIB_DIR=${REBAR_ROOT_LIB_DIR}")
endif()

if(TARGET rebar_ml_model_cf_moonshine_stt)
    set(REBAR_CF_CPP_TEST_TARGET rebar_ml_cf_cpp_tests)
    add_executable(${REBAR_CF_CPP_TEST_TARGET}
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/RebarComputeFlowTest.cpp)
    llm_configure_rpath(${REBAR_CF_CPP_TEST_TARGET})
    target_include_directories(${REBAR_CF_CPP_TEST_TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/src/cpp/src)
    target_compile_definitions(${REBAR_CF_CPP_TEST_TARGET} PRIVATE
        REBAR_ROOT_BIN_DIR="${REBAR_ROOT_BIN_DIR}"
        REBAR_ROOT_LIB_DIR="${REBAR_ROOT_LIB_DIR}")
    target_link_libraries(${REBAR_CF_CPP_TEST_TARGET} PUBLIC
        rebar_ml_core
        rebar_ml_model_wavtopcm
        rebar_ml_model_pcms16letof32le
        rebar_ml_model_cf_moonshine_stt
        Catch2::Catch2WithMain)

    add_test(NAME rebar_ml_cf_cpp_ctest COMMAND ${REBAR_CF_CPP_TEST_TARGET})
    set_tests_properties(rebar_ml_cf_cpp_ctest PROPERTIES
        ENVIRONMENT
        "REBAR_ROOT_BIN_DIR=${REBAR_ROOT_BIN_DIR};REBAR_ROOT_LIB_DIR=${REBAR_ROOT_LIB_DIR}")
endif()

if(TARGET rebar_ml_jni_core)
    message(STATUS "Adding Rebar JNI tests")

    include(find-java)

    set(REBAR_TEST_UTILS_DIR ${DOWNLOADS_DIR}/test_utils)
    file(GLOB _jarDependencies "${REBAR_TEST_UTILS_DIR}/*.jar")
    string(REPLACE ";" ":" _jarDependenciesTests "${_jarDependencies}")

    if(NOT TARGET rebar_ml_java)
        message(FATAL_ERROR "Rebar JNI tests require rebar_ml_java target; add rebar/core first")
    endif()

    get_target_property(_rebarJarFile rebar_ml_java JAR_FILE)

    add_jar(
            rebar_ml_jni_core_tests
            java/com/arm/rebarml/RebarTestJNI.java
            java/com/arm/rebarml/RebarTeePipelineTestJNI.java
            INCLUDE_JARS
            ${_jarDependencies}
            ${_rebarJarFile})

    add_dependencies(rebar_ml_jni_core_tests
        rebar_ml_java
        rebar_ml_jni_core
    )

    get_target_property(_jarFile rebar_ml_jni_core_tests JAR_FILE)
    get_target_property(_jniLibPath rebar_ml_jni_core LIBRARY_OUTPUT_DIRECTORY)

    add_test(
            NAME rebar_ml_jni_core_ctest
            COMMAND
            ${Java_JAVA_EXECUTABLE}
            -Djava.library.path=${_jniLibPath}
            -Dbackend.shared.lib.dir=${REBAR_ROOT_BIN_DIR}
            -Drebar.root.bin.dir=${REBAR_ROOT_BIN_DIR}
            -Drebar.root.lib.dir=${REBAR_ROOT_LIB_DIR}
            -cp ${_jarDependenciesTests}:${_rebarJarFile}:${_jarFile}
            org.junit.runner.JUnitCore
            com.arm.rebarml.RebarTestJNI)
    set_tests_properties(rebar_ml_jni_core_ctest PROPERTIES
        ENVIRONMENT
    "REBAR_ROOT_BIN_DIR=${REBAR_ROOT_BIN_DIR};REBAR_ROOT_LIB_DIR=${REBAR_ROOT_LIB_DIR}")

    add_test(
            NAME rebar_ml_jni_tee_pipeline_ctest
            COMMAND
            ${Java_JAVA_EXECUTABLE}
            -Djava.library.path=${_jniLibPath}
            -Dbackend.shared.lib.dir=${REBAR_ROOT_BIN_DIR}
            -Drebar.root.bin.dir=${REBAR_ROOT_BIN_DIR}
            -Drebar.root.lib.dir=${REBAR_ROOT_LIB_DIR}
            -cp ${_jarDependenciesTests}:${_rebarJarFile}:${_jarFile}
            org.junit.runner.JUnitCore
            com.arm.rebarml.RebarTeePipelineTestJNI)
    set_tests_properties(rebar_ml_jni_tee_pipeline_ctest PROPERTIES
        ENVIRONMENT
    "REBAR_ROOT_BIN_DIR=${REBAR_ROOT_BIN_DIR};REBAR_ROOT_LIB_DIR=${REBAR_ROOT_LIB_DIR}")

    if(TARGET rebar_ml_jni_model_wavtopcm AND TARGET rebar_ml_model_wavtopcm_java AND
       TARGET rebar_ml_jni_model_pcms16letof32le AND TARGET rebar_ml_model_pcms16letof32le_java)

        get_target_property(_rebarWavToPcmJarFile rebar_ml_model_wavtopcm_java JAR_FILE)
        get_target_property(_rebarPcmS16leToF32leJarFile rebar_ml_model_pcms16letof32le_java JAR_FILE)

        add_jar(
                rebar_ml_jni_models_tests
                java/com/arm/rebarml/RebarModelsTestJNI.java

                INCLUDE_JARS
                ${_jarDependencies}
                ${_rebarJarFile}
                ${_rebarWavToPcmJarFile}
                ${_rebarPcmS16leToF32leJarFile})

        add_dependencies(rebar_ml_jni_models_tests
            rebar_ml_java
            rebar_ml_model_wavtopcm_java
            rebar_ml_model_pcms16letof32le_java
            rebar_ml_jni_core
            rebar_ml_jni_model_wavtopcm
            rebar_ml_jni_model_pcms16letof32le)

        get_target_property(_modelsJarFile rebar_ml_jni_models_tests JAR_FILE)

        add_test(
                NAME rebar_ml_jni_models_ctest
                COMMAND
                ${Java_JAVA_EXECUTABLE}
                -Djava.library.path=${_jniLibPath}
                -Dbackend.shared.lib.dir=${REBAR_ROOT_BIN_DIR}
                -Drebar.root.bin.dir=${REBAR_ROOT_BIN_DIR}
                -Drebar.root.lib.dir=${REBAR_ROOT_LIB_DIR}
                -cp ${_jarDependenciesTests}:${_rebarJarFile}:${_rebarWavToPcmJarFile}:${_rebarPcmS16leToF32leJarFile}:${_modelsJarFile}
                org.junit.runner.JUnitCore
                com.arm.rebarml.RebarModelsTestJNI)
        set_tests_properties(rebar_ml_jni_models_ctest PROPERTIES
            ENVIRONMENT
            "REBAR_ROOT_BIN_DIR=${REBAR_ROOT_BIN_DIR};REBAR_ROOT_LIB_DIR=${REBAR_ROOT_LIB_DIR}")
    endif()

    if(TARGET rebar_ml_jni_model_cf_moonshine_stt AND TARGET rebar_ml_model_cf_moonshine_stt_java AND
       TARGET rebar_ml_jni_model_wavtopcm AND TARGET rebar_ml_model_wavtopcm_java AND
       TARGET rebar_ml_jni_model_pcms16letof32le AND TARGET rebar_ml_model_pcms16letof32le_java)
        get_target_property(_rebarWavToPcmJarFile rebar_ml_model_wavtopcm_java JAR_FILE)
        get_target_property(_rebarPcmS16leToF32leJarFile rebar_ml_model_pcms16letof32le_java JAR_FILE)
        get_target_property(_rebarCfMoonshineJarFile rebar_ml_model_cf_moonshine_stt_java JAR_FILE)

        add_jar(
                rebar_ml_jni_cf_tests
                java/com/arm/rebarml/RebarComputeFlowTestJNI.java

                INCLUDE_JARS
                ${_jarDependencies}
                ${_rebarJarFile}
                ${_rebarWavToPcmJarFile}
                ${_rebarPcmS16leToF32leJarFile}
                ${_rebarCfMoonshineJarFile})

        add_dependencies(rebar_ml_jni_cf_tests
            rebar_ml_java
            rebar_ml_model_wavtopcm_java
            rebar_ml_model_pcms16letof32le_java
            rebar_ml_model_cf_moonshine_stt_java
            rebar_ml_jni_core
            rebar_ml_jni_model_wavtopcm
            rebar_ml_jni_model_pcms16letof32le
            rebar_ml_jni_model_cf_moonshine_stt)

        get_target_property(_cfJarFile rebar_ml_jni_cf_tests JAR_FILE)

        add_test(
                NAME rebar_ml_jni_cf_ctest
                COMMAND
                ${Java_JAVA_EXECUTABLE}
                -Djava.library.path=${_jniLibPath}
                -Dbackend.shared.lib.dir=${REBAR_ROOT_BIN_DIR}
                -Drebar.root.bin.dir=${REBAR_ROOT_BIN_DIR}
                -Drebar.root.lib.dir=${REBAR_ROOT_LIB_DIR}
                -cp ${_jarDependenciesTests}:${_rebarJarFile}:${_rebarWavToPcmJarFile}:${_rebarPcmS16leToF32leJarFile}:${_rebarCfMoonshineJarFile}:${_cfJarFile}
                org.junit.runner.JUnitCore
                com.arm.rebarml.RebarComputeFlowTestJNI)
        set_tests_properties(rebar_ml_jni_cf_ctest PROPERTIES
            ENVIRONMENT
            "REBAR_ROOT_BIN_DIR=${REBAR_ROOT_BIN_DIR};REBAR_ROOT_LIB_DIR=${REBAR_ROOT_LIB_DIR}")
    endif()
endif()
