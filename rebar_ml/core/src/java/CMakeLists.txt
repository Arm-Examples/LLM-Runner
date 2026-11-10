#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

add_library(rebar_ml_java_interface INTERFACE)
target_sources(rebar_ml_java_interface INTERFACE
    com/arm/rebarml/AudioCodec.java
    com/arm/rebarml/AudioData.java
    com/arm/rebarml/AudioFormat.java
    com/arm/rebarml/AudioInput.java
    com/arm/rebarml/AudioInputPort.java
    com/arm/rebarml/AudioOutput.java
    com/arm/rebarml/AudioOutputPort.java
    com/arm/rebarml/ConnectionOptions.java
    com/arm/rebarml/ErrorCode.java
    com/arm/rebarml/NativeAudioReceiveResult.java
    com/arm/rebarml/NativeTextReceiveResult.java
    com/arm/rebarml/NativeResource.java
    com/arm/rebarml/OutputItem.java
    com/arm/rebarml/OverflowPolicy.java
    com/arm/rebarml/RebarML.java
    com/arm/rebarml/RebarBuffer.java
    com/arm/rebarml/RebarError.java
    com/arm/rebarml/RebarException.java
    com/arm/rebarml/RebarNode.java
    com/arm/rebarml/RebarNodeSession.java
    com/arm/rebarml/RebarPipelineSession.java
    com/arm/rebarml/ReceiveResult.java
    com/arm/rebarml/ReceiveStatus.java
    com/arm/rebarml/Tee.java
    com/arm/rebarml/TextData.java
    com/arm/rebarml/TextInputPort.java
    com/arm/rebarml/TextOutput.java
    com/arm/rebarml/TextOutputPort.java)

include(find-java)

get_target_property(REBAR_JAVA_INTERFACE_SOURCES
        rebar_ml_java_interface INTERFACE_SOURCES)

add_jar(
        rebar_ml_java
        ${REBAR_JAVA_INTERFACE_SOURCES}
        OUTPUT_DIR "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}"
        OUTPUT_NAME "rebar_ml")

if(TARGET rebar_ml_jni_core)
    add_dependencies(rebar_ml_java rebar_ml_jni_core)
endif()
