//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

abstract class NativeResource implements AutoCloseable {
    private long nativeHandle;

    protected NativeResource(long nativeHandle) {
        this.nativeHandle = nativeHandle;
    }

    protected final long nativeHandle() {
        if (nativeHandle == 0) {
            throw new IllegalStateException(getClass().getSimpleName() + " is closed");
        }
        return nativeHandle;
    }

    protected abstract void releaseNativeHandle(long nativeHandle);

    @Override
    public final void close() {
        long handle = nativeHandle;
        if (handle != 0) {
            nativeHandle = 0;
            releaseNativeHandle(handle);
        }
    }
}

