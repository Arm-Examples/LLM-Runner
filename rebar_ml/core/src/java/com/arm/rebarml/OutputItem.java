//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import java.util.Objects;

public final class OutputItem<T> {
    private final T data;
    private final boolean isFinal;

    public OutputItem(T data, boolean isFinal) {
        this.data = Objects.requireNonNull(data, "data");
        this.isFinal = isFinal;
    }

    public T data() { return data; }
    public boolean isFinal() { return isFinal; }
}
