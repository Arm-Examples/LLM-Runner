//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import java.nio.ByteBuffer;
import java.util.Objects;

public final class RebarBuffer implements AutoCloseable {
    private final ByteBuffer buffer;

    private RebarBuffer(ByteBuffer buffer) {
        this.buffer = buffer.asReadOnlyBuffer();
    }

    public static RebarBuffer wrapInput(ByteBuffer source) {
        Objects.requireNonNull(source, "source");
        ByteBuffer view = source.slice();
        if (!view.isDirect()) {
            ByteBuffer direct = ByteBuffer.allocateDirect(view.remaining());
            direct.put(view);
            direct.flip();
            view = direct;
        }
        return new RebarBuffer(view);
    }

    static RebarBuffer copyOutput(byte[] source) {
        ByteBuffer direct = ByteBuffer.allocateDirect(source.length);
        direct.put(source);
        direct.flip();
        return new RebarBuffer(direct);
    }

    public ByteBuffer buffer() {
        return buffer.asReadOnlyBuffer();
    }

    @Override
    public void close() {
        // MIGRATION TODO: this scaffold copies native output into Java-owned
        // direct memory. If you adopt a native buffer lease, release it here.
    }
}
