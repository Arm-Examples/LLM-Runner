//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import java.util.Objects;

public final class TextData {
    private final String text;
    private final String language;

    public TextData(String text, String language) {
        this.text = Objects.requireNonNull(text, "text");
        this.language = language;
    }

    public String text() { return text; }
    public String language() { return language; }
}
