//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

public final class Tee {
    private Tee() {}

    public static RebarNode createNode(int outputCount) {
        return RebarNode.fromNativeHandle(
                RebarML.createTeeNode(outputCount));
    }
}
