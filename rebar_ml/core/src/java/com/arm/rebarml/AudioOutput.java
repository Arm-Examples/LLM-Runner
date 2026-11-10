//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

public final class AudioOutput extends NativeResource {
    AudioOutput(long nativeHandle) { super(nativeHandle); }

    public ReceiveResult<AudioData> receive() {
        NativeAudioReceiveResult nativeResult =
                RebarML.audioOutputReceive(nativeHandle());

        if (nativeResult.status == 0) {
            if (nativeResult.bytes == null) return ReceiveResult.endOfStream();
            AudioCodec codec = AudioCodec.values()[nativeResult.codec];
            AudioData data = new AudioData(
                    RebarBuffer.copyOutput(nativeResult.bytes),
                    new AudioFormat(
                            codec,
                            nativeResult.sampleRateHz,
                            nativeResult.channels),
                    nativeResult.language);
            return ReceiveResult.success(new OutputItem<>(data, nativeResult.isFinal));
        }
        if (nativeResult.status == 2) return ReceiveResult.cancelled();
        return ReceiveResult.error(new RebarError(
                ErrorCode.fromNative(nativeResult.errorCode),
                nativeResult.errorMessage == null
                        ? "native audio output failed"
                        : nativeResult.errorMessage));
    }

    public void cancel() {
        RebarML.audioOutputCancel(nativeHandle());
    }

    @Override
    protected void releaseNativeHandle(long nativeHandle) {
        RebarML.releaseAudioOutput(nativeHandle);
    }
}
