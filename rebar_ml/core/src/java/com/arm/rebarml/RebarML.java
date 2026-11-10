//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import java.nio.ByteBuffer;

final class RebarML {
    static {
        System.loadLibrary("rebar_ml_jni_core");
    }

    private RebarML() {}

    static native void releaseNode(long handle);
    static native long createTeeNode(int outputCount);

    static native long getNodeAudioInputPort(long nodeHandle, int index);
    static native long getNodeAudioOutputPort(long nodeHandle, int index);
    static native long getNodeTextInputPort(long nodeHandle, int index);
    static native long getNodeTextOutputPort(long nodeHandle, int index);
    static native void releaseAudioInputPort(long handle);
    static native void releaseAudioOutputPort(long handle);
    static native void releaseTextInputPort(long handle);
    static native void releaseTextOutputPort(long handle);
    static native void connectAudioPorts(
            long outputHandle,
            long inputHandle,
            int queueCapacity,
            int overflowPolicy);
    static native void connectTextPorts(
            long outputHandle,
            long inputHandle,
            int queueCapacity,
            int overflowPolicy);

    static native long createPipelineSession(long nodeHandle);
    static native void releasePipelineSession(long handle);
    static native long getPipelineSessionNodeSession(long pipelineSessionHandle, long nodeHandle);
    static native void submitPipelineSession(long pipelineSessionHandle);
    static native void finishPipelineSessionInputs(long pipelineSessionHandle);
    static native void teardownPipelineSession(long pipelineSessionHandle);
    static native void cancelPipelineSession(long pipelineSessionHandle);
    static native boolean isPipelineSessionCancelled(long pipelineSessionHandle);

    static native void releaseNodeSession(long handle);
    static native long getNodeSessionAudioInput(long sessionHandle, int index);
    static native long getNodeSessionTextOutput(long sessionHandle, int index);
    static native long getNodeSessionAudioOutput(long sessionHandle, int index);

    static native void releaseAudioInput(long handle);
    static native void audioInputAdd(
            long handle,
            ByteBuffer bytes,
            int offset,
            int length,
            int codec,
            int sampleRateHz,
            int channels,
            String language);
    static native void audioInputSend(
            long handle,
            ByteBuffer bytes,
            int offset,
            int length,
            int codec,
            int sampleRateHz,
            int channels,
            String language);

    static native void releaseTextOutput(long handle);
    static native NativeTextReceiveResult textOutputReceive(long handle);
    static native void textOutputCancel(long handle);

    static native void releaseAudioOutput(long handle);
    static native NativeAudioReceiveResult audioOutputReceive(long handle);
    static native void audioOutputCancel(long handle);
}
