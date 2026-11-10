//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

public final class AudioInputPort extends NativeResource {
    AudioInputPort(long nativeHandle) { super(nativeHandle); }

    long getNativeHandle() {
        return nativeHandle();
    }

    @Override
    protected void releaseNativeHandle(long nativeHandle) {
        RebarML.releaseAudioInputPort(nativeHandle);
    }
}
