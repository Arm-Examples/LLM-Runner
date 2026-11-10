//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

public final class RebarNode extends NativeResource {
    private static long requireNativeHandle(long nativeHandle) {
        if (nativeHandle == 0) {
            throw new RebarException(new RebarError(
                    ErrorCode.BACKEND_FAILURE,
                    "native node creation failed"));
        }
        return nativeHandle;
    }

    private RebarNode(long nativeHandle) {
        super(requireNativeHandle(nativeHandle));
    }

    public static RebarNode fromNativeHandle(long nativeHandle) {
        return new RebarNode(nativeHandle);
    }

    public AudioInputPort getAudioInputPort() { return getAudioInputPort(0); }
    public AudioInputPort getAudioInputPort(int index) {
        long handle = RebarML.getNodeAudioInputPort(getNativeHandle(), index);
        return handle == 0 ? null : new AudioInputPort(handle);
    }

    public AudioOutputPort getAudioOutputPort() { return getAudioOutputPort(0); }
    public AudioOutputPort getAudioOutputPort(int index) {
        long handle = RebarML.getNodeAudioOutputPort(getNativeHandle(), index);
        return handle == 0 ? null : new AudioOutputPort(handle);
    }

    public TextInputPort getTextInputPort() { return getTextInputPort(0); }
    public TextInputPort getTextInputPort(int index) {
        long handle = RebarML.getNodeTextInputPort(getNativeHandle(), index);
        return handle == 0 ? null : new TextInputPort(handle);
    }

    public TextOutputPort getTextOutputPort() { return getTextOutputPort(0); }
    public TextOutputPort getTextOutputPort(int index) {
        long handle = RebarML.getNodeTextOutputPort(getNativeHandle(), index);
        return handle == 0 ? null : new TextOutputPort(handle);
    }

    public RebarPipelineSession createPipelineSession() {
        return new RebarPipelineSession(
                RebarML.createPipelineSession(getNativeHandle()));
    }

    long getNativeHandle() {
        return nativeHandle();
    }

    @Override
    protected void releaseNativeHandle(long nativeHandle) {
        RebarML.releaseNode(nativeHandle);
    }
}
