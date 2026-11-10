//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import java.util.Objects;

public final class AudioData implements AutoCloseable {
    private final RebarBuffer bytes;
    private final AudioFormat format;
    private final String language;

    public AudioData(RebarBuffer bytes, AudioFormat format, String language) {
        this.bytes = Objects.requireNonNull(bytes, "bytes");
        this.format = Objects.requireNonNull(format, "format");
        this.language = language;
    }

    public RebarBuffer bytes() { return bytes; }
    public AudioFormat format() { return format; }
    public String language() { return language; }

    @Override
    public void close() {
        bytes.close();
    }
}
