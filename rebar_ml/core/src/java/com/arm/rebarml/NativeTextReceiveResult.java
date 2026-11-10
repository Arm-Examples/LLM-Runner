//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

final class NativeTextReceiveResult {
    final int status;
    final String text;
    final String language;
    final boolean isFinal;
    final int errorCode;
    final String errorMessage;

    NativeTextReceiveResult(
            int status,
            String text,
            String language,
            boolean isFinal,
            int errorCode,
            String errorMessage) {
        this.status = status;
        this.text = text;
        this.language = language;
        this.isFinal = isFinal;
        this.errorCode = errorCode;
        this.errorMessage = errorMessage;
    }
}
