//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

public enum ErrorCode {
    UNKNOWN,
    INVALID_ARGUMENT,
    UNSUPPORTED_FORMAT,
    ALREADY_CONNECTED,
    INCOMPATIBLE_CONNECTION,
    GRAPH_CYCLE,
    GRAPH_IN_USE,
    INVALID_GRAPH,
    NODE_NOT_IN_PIPELINE,
    PORT_NOT_AVAILABLE,
    INPUT_PENDING,
    INPUT_CLOSED,
    OUTPUT_CLOSED,
    CANCELLED,
    BACKEND_FAILURE,
    NOT_IMPLEMENTED,
    QUEUE_FULL;

    static ErrorCode fromNative(int value) {
        ErrorCode[] values = values();
        return value >= 0 && value < values.length ? values[value] : UNKNOWN;
    }
}
