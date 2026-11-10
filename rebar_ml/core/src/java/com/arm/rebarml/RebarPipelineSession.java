//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import java.util.Objects;

public final class RebarPipelineSession extends NativeResource {
    private static long requireNativeHandle(long nativeHandle) {
        if (nativeHandle == 0) {
            throw new RebarException(
                    new RebarError(ErrorCode.INVALID_GRAPH, "native pipeline session creation failed"));
        }
        return nativeHandle;
    }

    RebarPipelineSession(long nativeHandle) { super(requireNativeHandle(nativeHandle)); }

    public RebarNodeSession getNodeSession(RebarNode node) {
        Objects.requireNonNull(node, "node");
        return new RebarNodeSession(
                RebarML.getPipelineSessionNodeSession(getNativeHandle(), node.getNativeHandle()));
    }

    public void submit() { RebarML.submitPipelineSession(getNativeHandle()); }
    public void finishInputs() { RebarML.finishPipelineSessionInputs(getNativeHandle()); }
    public void teardown() { RebarML.teardownPipelineSession(getNativeHandle()); }
    public void cancel() { RebarML.cancelPipelineSession(getNativeHandle()); }
    public boolean isCancelled() { return RebarML.isPipelineSessionCancelled(getNativeHandle()); }

    long getNativeHandle() {
        return nativeHandle();
    }

    @Override
    protected void releaseNativeHandle(long nativeHandle) {
        RebarML.releasePipelineSession(nativeHandle);
    }
}
