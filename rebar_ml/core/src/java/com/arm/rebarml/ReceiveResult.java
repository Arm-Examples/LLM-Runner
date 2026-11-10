//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

public final class ReceiveResult<T> {
    private final ReceiveStatus status;
    private final OutputItem<T> item;
    private final RebarError error;

    private ReceiveResult(ReceiveStatus status, OutputItem<T> item, RebarError error) {
        this.status = status;
        this.item = item;
        this.error = error;
    }

    public static <T> ReceiveResult<T> success(OutputItem<T> item) {
        return new ReceiveResult<>(ReceiveStatus.SUCCESS, item, null);
    }

    public static <T> ReceiveResult<T> endOfStream() {
        return new ReceiveResult<>(ReceiveStatus.SUCCESS, null, null);
    }

    public static <T> ReceiveResult<T> error(RebarError error) {
        return new ReceiveResult<>(ReceiveStatus.ERROR, null, error);
    }

    public static <T> ReceiveResult<T> cancelled() {
        return new ReceiveResult<>(ReceiveStatus.CANCELLED, null, null);
    }

    public ReceiveStatus status() { return status; }
    public OutputItem<T> item() { return item; }
    public RebarError error() { return error; }
}
