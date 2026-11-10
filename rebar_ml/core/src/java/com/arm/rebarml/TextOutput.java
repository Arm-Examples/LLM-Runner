//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

public final class TextOutput extends NativeResource {
    TextOutput(long nativeHandle) { super(nativeHandle); }

    public ReceiveResult<TextData> receive() {
        NativeTextReceiveResult nativeResult = RebarML.textOutputReceive(nativeHandle());
        if (nativeResult.status == 0) {
            if (nativeResult.text == null) return ReceiveResult.endOfStream();
            return ReceiveResult.success(new OutputItem<>(
                    new TextData(nativeResult.text, nativeResult.language),
                    nativeResult.isFinal));
        }
        if (nativeResult.status == 2) return ReceiveResult.cancelled();
        return ReceiveResult.error(new RebarError(
                ErrorCode.fromNative(nativeResult.errorCode),
                nativeResult.errorMessage == null ? "native output failure" : nativeResult.errorMessage));
    }

    public void cancel() { RebarML.textOutputCancel(nativeHandle()); }

    @Override
    protected void releaseNativeHandle(long nativeHandle) {
        RebarML.releaseTextOutput(nativeHandle);
    }
}
