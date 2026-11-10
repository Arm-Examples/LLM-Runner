//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

final class NativeAudioReceiveResult {
    final int status;
    final byte[] bytes;
    final int codec;
    final int sampleRateHz;
    final int channels;
    final String language;
    final boolean isFinal;
    final int errorCode;
    final String errorMessage;

    NativeAudioReceiveResult(
            int status,
            byte[] bytes,
            int codec,
            int sampleRateHz,
            int channels,
            String language,
            boolean isFinal,
            int errorCode,
            String errorMessage) {
        this.status = status;
        this.bytes = bytes;
        this.codec = codec;
        this.sampleRateHz = sampleRateHz;
        this.channels = channels;
        this.language = language;
        this.isFinal = isFinal;
        this.errorCode = errorCode;
        this.errorMessage = errorMessage;
    }
}
