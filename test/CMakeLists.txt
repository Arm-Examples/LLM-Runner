#
# SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Declare project
project(llm-tests
        VERSION     0.0.1
        DESCRIPTION "LLM wrapper tests")

set(CATCH_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE STRING "Catch2 location")

set(TEST_MODELS_DIR ${DOWNLOADS_DIR}/models)
set(TEST_RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test_resources)
set(TEST_UTILS_DIR  ${DOWNLOADS_DIR}/test_utils)
set(TEST_MODELS_DIR  ${DOWNLOADS_DIR}/models)


if (NOT TARGET llm-cpp)
    message(FATAL_ERROR "llm-cpp-tests requires target llm-cpp")
endif()

message(STATUS "Adding C++ tests")

set(CONFIG_FILE_NAME "")
if (LLM_ENABLE_LLAMA_CPP)
    list(APPEND CONFIG_FILE_NAME
        "llamaTextConfig-phi-2.json"
        "llamaVisionConfig-qwen2-vl-2B.json"
        "llamaTextConfig-llama-3.2-1B.json")
endif()
if (LLM_ENABLE_ONNXRUNTIME_GENAI)
    list(APPEND CONFIG_FILE_NAME "onnxrtTextConfig-phi-4.json")
endif()
if (LLM_ENABLE_MNN)
    list(APPEND CONFIG_FILE_NAME
        "mnnTextConfig-llama-3.2-1B.json"
        "mnnVisionConfig-qwen2.5-3B.json")
endif()
if (LLM_ENABLE_EXECUTORCH)
    list(APPEND CONFIG_FILE_NAME "executorchTextConfig-llama-3.2-1B.json")
endif()

list(LENGTH CONFIG_FILE_NAME _llm_config_count)
if (_llm_config_count EQUAL 0)
    message(FATAL_ERROR "No test configs selected; check LLM_ENABLE_* options.")
endif()

if (NOT DOWNLOAD_LLM_MODELS)
    foreach(_backend ${LLM_ENABLED_BACKENDS})
        if (NOT EXISTS "${TEST_MODELS_DIR}/${_backend}")
            message(FATAL_ERROR
                "Model downloads are disabled (DOWNLOAD_LLM_MODELS=OFF) but no models were found under "
                "${TEST_MODELS_DIR}/${_backend}. Reconfigure without -DDOWNLOAD_LLM_MODELS=OFF or "
                "disable BUILD_LLM_TESTING.")
        endif()
    endforeach()
endif()

# Download Catch and add to the build.
include(find-catch)

# Add test to CTest for running it automatically
enable_testing()

set(TEST_TARGET llm-cpp-tests)
set(TEST_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/LlmTest.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/LlmConfigTest.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/LlmLogTest.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/BenchRunnerTest.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/LlmSwitchEndToEndTest.cpp)

if (LLM_ENABLE_MNN OR LLM_ENABLE_LLAMA_CPP)
    list(APPEND TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/cpp/ImageUtilsTest.cpp)
endif()

add_executable(${TEST_TARGET} ${TEST_SOURCES})

target_compile_definitions(${TEST_TARGET}
        PUBLIC
        TEST_MODELS_DIR="${TEST_MODELS_DIR}"
        TEST_RESOURCE_DIR="${TEST_RESOURCE_DIR}"
        MODEL_CONFIG_DIR="${CMAKE_SOURCE_DIR}/config_files/model_configuration_files"
        ACTIVE_LOG_LEVEL=LOG_LEVEL_WARN)

target_include_directories(${TEST_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/cpp
        ${CMAKE_SOURCE_DIR}/src/cpp/frameworks)

target_include_directories(${TEST_TARGET} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/cpp/benchmark)

# Android specific definition for Catch
if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    target_compile_definitions(${TEST_TARGET} PRIVATE
            "CATCH_CONFIG_NO_ANDROID_LOGWRITE")
endif()

# Assert dependency on the actual C++ interface or library.
target_link_libraries(${TEST_TARGET} PUBLIC llm-cpp llm-bench Catch2::Catch2)

if (CMAKE_SYSTEM_NAME STREQUAL "Android")
    target_link_libraries(${TEST_TARGET} PRIVATE log)
endif()

foreach(CFG_FILE ${CONFIG_FILE_NAME})

    message(STATUS "Adding cpp test for ${CFG_FILE}")
    # We pass in the compile definition for location of test
    # models directory to know where to read the model file
    # from.
    string(TOLOWER "${CFG_FILE}" CFG_FILE_EXT)
    string(MAKE_C_IDENTIFIER "${CFG_FILE_EXT}" CFG_FILE_EXT)

    set(CONFIG_FILE_SOURCE "${CMAKE_SOURCE_DIR}/config_files/model_configuration_files/${CFG_FILE}")

    if(NOT EXISTS ${CONFIG_FILE_SOURCE})
        message(FATAL_ERROR "Configuration file not found: ${CONFIG_FILE_SOURCE}")
    endif()

    # Add test to CTest for running it automatically
    add_test(NAME llm-cpp-ctest-${CFG_FILE_EXT} COMMAND ${TEST_TARGET}
            --config ${CONFIG_FILE_SOURCE}
            --model-root ${TEST_MODELS_DIR}
            --backend-shared-lib-dir "${CMAKE_BINARY_DIR}/bin")
endforeach()

# If JNI libs are being built, add tests for these here.
if(TARGET llm-jni)
    message(STATUS "Adding JNI tests")

    if (NOT TARGET llm-java)
        message(FATAL_ERROR "JNI tests cannot be built without Java interface")
    endif()

    # Find JAVA and add UseJava CMake module.
    include(find-java)

    # Find all the JAR files (dependencies) that have been downloaded.
    file(GLOB _jarDependencies "${TEST_UTILS_DIR}/*.jar")

    # Get the Java interface source file path.
    get_target_property(LLM_JAVA_INTERFACE_SOURCES
            llm-java INTERFACE_SOURCES)
    message(STATUS "Java interface sources: ${LLM_JAVA_INTERFACE_SOURCES}")

    # Build Java archive using the interface and test sources.
    add_jar(
            llm-jni-tests
            ${LLM_JAVA_INTERFACE_SOURCES}
            java/com/arm/LlmTestJNI.java

            INCLUDE_JARS
            ${_jarDependencies})

    get_target_property(_jarFile llm-jni-tests JAR_FILE)
    get_target_property(_classDir llm-jni-tests CLASSDIR)
    get_target_property(_jniLibPath llm-jni LIBRARY_OUTPUT_DIRECTORY)

    message(STATUS "Jar file:          ${_jarFile}")
    message(STATUS "Class compiled to: ${_classDir}")
    message(STATUS "JNI lib directory: ${_jniLibPath}")
    message(STATUS "Jar dependencies:  ${_jarDependencies}")

    string(REPLACE ";" ":" _jarDependenciesTests "${_jarDependencies}")
    foreach(CFG_FILE ${CONFIG_FILE_NAME})
        string(TOLOWER "${CFG_FILE}" CFG_FILE_EXT)
        string(MAKE_C_IDENTIFIER "${CFG_FILE_EXT}" CFG_FILE_EXT)

        set(CONFIG_FILE_SOURCE "${CMAKE_SOURCE_DIR}/config_files/model_configuration_files/${CFG_FILE}")
        set(CONFIG_FILE_DEST "${CMAKE_BINARY_DIR}/${CFG_FILE}")

        add_test(
            NAME llm-jni-ctest-${CFG_FILE_EXT}
            COMMAND
            ${Java_JAVA_EXECUTABLE}
            -Djava.library.path=${_jniLibPath}
            -Dbackend.shared.lib.dir=${CMAKE_BINARY_DIR}/bin
            -Dmodel_dir=${TEST_MODELS_DIR}
            -Dconfig_file=${CONFIG_FILE_SOURCE}
            -Dtest_image=${TEST_RESOURCE_DIR}
            -Duser_config_file=${USER_CONFIG_FILE_DEST}
            -cp ${_jarDependenciesTests}:${_jarFile}
            org.junit.runner.JUnitCore
            com.arm.LlmTestJNI)
        set_tests_properties(llm-jni-ctest-${CFG_FILE_EXT} PROPERTIES
            ENVIRONMENT "LLM_CONFIG_DIR=${CMAKE_SOURCE_DIR}/config_files")
    endforeach()
endif()
