//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import java.util.Objects;

public final class RebarError implements java.io.Serializable {
    private static final long serialVersionUID = 1L;
    private final ErrorCode code;
    private final String message;

    public RebarError(ErrorCode code, String message) {
        this.code = Objects.requireNonNull(code, "code");
        this.message = Objects.requireNonNull(message, "message");
    }

    public ErrorCode code() { return code; }
    public String message() { return message; }

    @Override
    public String toString() { return code + ": " + message; }
}
