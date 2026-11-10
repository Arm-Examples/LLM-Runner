//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import java.util.Objects;

public final class AudioFormat {
    private final AudioCodec codec;
    private final int sampleRateHz;
    private final int channels;

    public AudioFormat(AudioCodec codec, int sampleRateHz, int channels) {
        this.codec = Objects.requireNonNull(codec, "codec");
        if (sampleRateHz < 0) throw new IllegalArgumentException("sampleRateHz must be non-negative");
        if (channels < 0) throw new IllegalArgumentException("channels must be non-negative");
        this.sampleRateHz = sampleRateHz;
        this.channels = channels;
    }

    public AudioCodec codec() { return codec; }
    public int sampleRateHz() { return sampleRateHz; }
    public int channels() { return channels; }
}
