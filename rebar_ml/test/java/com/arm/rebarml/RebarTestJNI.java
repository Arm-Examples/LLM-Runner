//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertFalse;

import org.junit.Test;

public class RebarTestJNI {

    @Test
    public void testRootBinaryDirsProvided() {
        String rootBinDir = System.getProperty("rebar.root.bin.dir");
        String rootLibDir = System.getProperty("rebar.root.lib.dir");
        assertNotNull(rootBinDir);
        assertFalse(rootBinDir.isEmpty());
        assertNotNull(rootLibDir);
        assertFalse(rootLibDir.isEmpty());
    }

    @Test
    public void testTeeNodePortsAndOutputs() {
        try (RebarNode tee = Tee.createNode(2);
             TextInputPort teeIn = tee.getTextInputPort();
             TextOutputPort teeOut0 = tee.getTextOutputPort(0);
             TextOutputPort teeOut1 = tee.getTextOutputPort(1)) {
            assertNotNull(teeIn);
            assertNotNull(teeOut0);
            assertNotNull(teeOut1);
            assertNull(tee.getTextOutputPort(2));

            try (RebarPipelineSession pipelineSession = tee.createPipelineSession();
                 RebarNodeSession session = pipelineSession.getNodeSession(tee);
                 TextOutput out0 = session.getTextOutput(0);
                 TextOutput out1 = session.getTextOutput(1)) {
                assertNotNull(out0);
                assertNotNull(out1);

                pipelineSession.finishInputs();

                ReceiveResult<TextData> end0 = out0.receive();
                assertEquals(ReceiveStatus.SUCCESS, end0.status());
                assertNull(end0.item());

                ReceiveResult<TextData> end1 = out1.receive();
                assertEquals(ReceiveStatus.SUCCESS, end1.status());
                assertNull(end1.item());
            }
        }
    }
}
