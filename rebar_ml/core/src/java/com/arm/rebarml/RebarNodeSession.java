//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

public final class RebarNodeSession extends NativeResource {
    private static long requireNativeHandle(long nativeHandle) {
        if (nativeHandle == 0) {
            throw new RebarException(new RebarError(
                    ErrorCode.NODE_NOT_IN_PIPELINE,
                    "node session is unavailable"));
        }
        return nativeHandle;
    }

    RebarNodeSession(long nativeHandle) { super(requireNativeHandle(nativeHandle)); }

    public AudioInput getAudioInput() { return getAudioInput(0); }
    public AudioInput getAudioInput(int index) {
        long handle = RebarML.getNodeSessionAudioInput(getNativeHandle(), index);
        return handle == 0 ? null : new AudioInput(handle);
    }

    public TextOutput getTextOutput() { return getTextOutput(0); }
    public TextOutput getTextOutput(int index) {
        long handle = RebarML.getNodeSessionTextOutput(getNativeHandle(), index);
        return handle == 0 ? null : new TextOutput(handle);
    }

    public AudioOutput getAudioOutput() { return getAudioOutput(0); }
    public AudioOutput getAudioOutput(int index) {
        long handle = RebarML.getNodeSessionAudioOutput(getNativeHandle(), index);
        return handle == 0 ? null : new AudioOutput(handle);
    }

    long getNativeHandle() {
        return nativeHandle();
    }

    @Override
    protected void releaseNativeHandle(long nativeHandle) {
        RebarML.releaseNodeSession(nativeHandle);
    }
}
