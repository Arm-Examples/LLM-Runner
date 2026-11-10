//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import java.util.Objects;

public final class TextOutputPort extends NativeResource {
    TextOutputPort(long nativeHandle) { super(nativeHandle); }

    public void connect(TextInputPort destination) {
        connect(destination, ConnectionOptions.DEFAULT);
    }

    public void connect(TextInputPort destination, ConnectionOptions options) {
        Objects.requireNonNull(destination, "destination");
        Objects.requireNonNull(options, "options");
        RebarML.connectTextPorts(
                nativeHandle(),
                destination.getNativeHandle(),
                options.queueCapacity(),
                options.overflowPolicy().ordinal());
    }

    @Override
    protected void releaseNativeHandle(long nativeHandle) {
        RebarML.releaseTextOutputPort(nativeHandle);
    }
}
