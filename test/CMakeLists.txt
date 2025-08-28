#
# SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Declare project
project(llm-tests
        VERSION     0.0.1
        DESCRIPTION "LLM wrapper tests")

set(TEST_TARGET_NAME llm-cpp-tests)
set(CATCH_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE STRING "Catch2 location")

set(TEST_MODELS_DIR ${DOWNLOADS_DIR}/models CACHE STRING "Directory with required model resources")
set(TEST_UTILS_DIR  ${DOWNLOADS_DIR}/test_utils CACHE STRING "Directory with required test utilities jar resources")


if (NOT TARGET arm-llm-cpp)
    message(FATAL_ERROR "llm-cpp-tests requires target arm-llm-cpp")
endif()

message(STATUS "Adding C++ tests")
add_executable(llm-cpp-tests
        ${CMAKE_CURRENT_SOURCE_DIR}/cpp/LlmTest.cpp)

if (${LLM_FRAMEWORK} STREQUAL "llama.cpp")
    set(CONFIG_FILE_NAME "llamaConfig.json" CACHE STRING "Path to the Llama config file")
elseif (${LLM_FRAMEWORK} STREQUAL "onnxruntime-genai")
    set(CONFIG_FILE_NAME "onnxrtConfig.json" CACHE STRING "Path to the ONNX Runtime GenAI config file")
elseif (${LLM_FRAMEWORK} STREQUAL "mediapipe")
    set(CONFIG_FILE_NAME "mediapipeConfig.json" CACHE STRING "Path to the mediapipe config file")
endif ()


# We pass in the compile definition for location of test
# models directory to know where to read the model file
# from.
target_compile_definitions(llm-cpp-tests
        PUBLIC
        TEST_MODELS_DIR="${TEST_MODELS_DIR}")

# Android specific definition for Catch
if (${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    target_compile_definitions(llm-cpp-tests PRIVATE
            "CATCH_CONFIG_NO_ANDROID_LOGWRITE")
endif()

# Assert dependency on the actual C++ interface or library.
target_link_libraries(llm-cpp-tests PUBLIC arm-llm-cpp)

# Download Catch and add to the build.
include(find-catch)

# Include Catch header directory.
target_include_directories(llm-cpp-tests PUBLIC ${CATCH_DIR})

# Add test to CTest for running it automatically
enable_testing()
add_test(NAME llm-cpp-ctest COMMAND llm-cpp-tests)

set(CONFIG_FILE_SOURCE "${CMAKE_SOURCE_DIR}/model_configuration_files/${CONFIG_FILE_NAME}")
set(CONFIG_FILE_DEST "${CMAKE_BINARY_DIR}/${CONFIG_FILE_NAME}")

if(NOT EXISTS ${CONFIG_FILE_SOURCE})
    message(FATAL_ERROR "Configuration file not found: ${CONFIG_FILE_SOURCE}")
endif()

configure_file(${CONFIG_FILE_SOURCE} ${CONFIG_FILE_DEST} COPYONLY)

target_compile_definitions(llm-cpp-tests
        PUBLIC CONFIG_FILE_PATH="${CONFIG_FILE_DEST}")

# If JNI libs are being built, add tests for these here.
if(TARGET arm-llm-jni)
    message(STATUS "Adding JNI tests")

    if (NOT TARGET arm-llm-java)
        message(FATAL_ERROR "JNI tests cannot be built without Java interface")
    endif()

    # Find JAVA and add UseJava CMake module.
    include(find-java)

    # Find all the JAR files (dependencies) that have been downloaded.
    file(GLOB _jarDependencies "${TEST_UTILS_DIR}/*.jar")

    # Get the Java interface source file path.
    get_target_property(LLM_JAVA_INTERFACE_SOURCES
            arm-llm-java INTERFACE_SOURCES)
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
    get_target_property(_jniLibPath arm-llm-jni LIBRARY_OUTPUT_DIRECTORY)

    message(STATUS "Jar file:          ${_jarFile}")
    message(STATUS "Class compiled to: ${_classDir}")
    message(STATUS "JNI lib directory: ${_jniLibPath}")
    message(STATUS "Jar dependencies:  ${_jarDependencies}")

    string(REPLACE ";" ":" _jarDependenciesTests "${_jarDependencies}")
    add_test(
        NAME llm-jni-ctest
        COMMAND
        ${Java_JAVA_EXECUTABLE}
        -Djava.library.path=${_jniLibPath}
        -Dmodel_dir=${TEST_MODELS_DIR}
        -Dconfig_file=${CONFIG_FILE_DEST}
        -Duser_config_file=${USER_CONFIG_FILE_DEST}
        -cp ${_jarDependenciesTests}:${_jarFile}
        org.junit.runner.JUnitCore
        com.arm.LlmTestJNI)
endif()
