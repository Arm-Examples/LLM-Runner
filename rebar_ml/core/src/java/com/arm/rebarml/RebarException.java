//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

public final class RebarException extends RuntimeException {
    private static final long serialVersionUID = 1L;
    private final RebarError error;

    public RebarException(int nativeCode, String message) {
        this(new RebarError(ErrorCode.fromNative(nativeCode), message));
    }

    public RebarException(RebarError error) {
        super(error.toString());
        this.error = error;
    }

    public RebarError error() { return error; }
}
