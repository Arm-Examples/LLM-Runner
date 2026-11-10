//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import java.nio.ByteBuffer;
import java.util.Objects;

public final class AudioInput extends NativeResource {
    AudioInput(long nativeHandle) { super(nativeHandle); }

    public void add(AudioData data) { sendInternal(data, false); }
    public void send(AudioData data) { sendInternal(data, true); }

    private void sendInternal(AudioData data, boolean immediate) {
        Objects.requireNonNull(data, "data");
        ByteBuffer buffer = data.bytes().buffer();
        AudioFormat format = data.format();
        if (immediate) {
            RebarML.audioInputSend(
                    nativeHandle(), buffer, buffer.position(), buffer.remaining(),
                    format.codec().ordinal(), format.sampleRateHz(), format.channels(),
                    data.language());
        } else {
            RebarML.audioInputAdd(
                    nativeHandle(), buffer, buffer.position(), buffer.remaining(),
                    format.codec().ordinal(), format.sampleRateHz(), format.channels(),
                    data.language());
        }
    }

    @Override
    protected void releaseNativeHandle(long nativeHandle) {
        RebarML.releaseAudioInput(nativeHandle);
    }
}
