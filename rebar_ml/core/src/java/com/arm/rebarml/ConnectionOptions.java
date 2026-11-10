//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import java.util.Objects;

public final class ConnectionOptions {
    public static final ConnectionOptions DEFAULT = new ConnectionOptions(16, OverflowPolicy.BLOCK);

    private final int queueCapacity;
    private final OverflowPolicy overflowPolicy;

    public ConnectionOptions(int queueCapacity, OverflowPolicy overflowPolicy) {
        if (queueCapacity <= 0) throw new IllegalArgumentException("queueCapacity must be positive");
        this.queueCapacity = queueCapacity;
        this.overflowPolicy = Objects.requireNonNull(overflowPolicy, "overflowPolicy");
    }

    public int queueCapacity() { return queueCapacity; }
    public OverflowPolicy overflowPolicy() { return overflowPolicy; }
}
