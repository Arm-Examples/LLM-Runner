//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm.rebarml;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import java.nio.ByteBuffer;

import org.junit.Test;

public class RebarTeePipelineTestJNI {

    private static AudioData makePcmS16le(byte[] bytes, String language) {
        return new AudioData(
                RebarBuffer.wrapInput(ByteBuffer.wrap(bytes)),
                new AudioFormat(AudioCodec.PCM_S16LE, /*sampleRateHz*/ 16000, /*channels*/ 1),
                language);
    }

    private static void assertAudioEquals(AudioData expected, AudioData actual) {
        assertNotNull(expected);
        assertNotNull(actual);

        assertEquals(expected.format().codec(), actual.format().codec());
        assertEquals(expected.format().sampleRateHz(), actual.format().sampleRateHz());
        assertEquals(expected.format().channels(), actual.format().channels());
        assertEquals(expected.language(), actual.language());

        ByteBuffer expectedBuf = expected.bytes().buffer();
        byte[] expectedBytes = new byte[expectedBuf.remaining()];
        expectedBuf.get(expectedBytes);

        ByteBuffer actualBuf = actual.bytes().buffer();
        byte[] actualBytes = new byte[actualBuf.remaining()];
        actualBuf.get(actualBytes);

        assertArrayEquals(expectedBytes, actualBytes);
    }

    @Test
    public void testThreeTeeNodesAudioFlow() {
        try (RebarNode tee0 = Tee.createNode(2);
             RebarNode tee1 = Tee.createNode(2);
             RebarNode tee2 = Tee.createNode(2);
             AudioOutputPort tee0Out0 = tee0.getAudioOutputPort(0);
             AudioOutputPort tee1Out0 = tee1.getAudioOutputPort(0);
             AudioOutputPort tee0TapPort = tee0.getAudioOutputPort(1);
             AudioOutputPort tee1TapPort = tee1.getAudioOutputPort(1);
             AudioOutputPort tee2Out0 = tee2.getAudioOutputPort(0);
             AudioOutputPort tee2TapPort = tee2.getAudioOutputPort(1);
             AudioInputPort tee1In = tee1.getAudioInputPort();
             AudioInputPort tee2In = tee2.getAudioInputPort()) {

            tee0Out0.connect(tee1In);
            tee1Out0.connect(tee2In);

            try (RebarPipelineSession pipelineSession = tee0.createPipelineSession();
                 RebarNodeSession tee0Session = pipelineSession.getNodeSession(tee0);
                 RebarNodeSession tee1Session = pipelineSession.getNodeSession(tee1);
                 RebarNodeSession tee2Session = pipelineSession.getNodeSession(tee2);
                 AudioInput input = tee0Session.getAudioInput();
                 AudioOutput tee0Tap = tee0Session.getAudioOutput(1);
                 AudioOutput tee1Tap = tee1Session.getAudioOutput(1);
                 AudioOutput tee2Out = tee2Session.getAudioOutput(0);
                 AudioOutput tee2Tap = tee2Session.getAudioOutput(1)) {

                assertNotNull(input);
                assertNull(tee0Session.getAudioOutput(0));
                assertNotNull(tee0Tap);
                assertNull(tee1Session.getAudioOutput(0));
                assertNotNull(tee1Tap);
                assertNotNull(tee2Out);
                assertNotNull(tee2Tap);

                assertNull(tee1Session.getAudioInput(0));
                assertNull(tee2Session.getAudioInput(0));

                try (AudioData frame0 = makePcmS16le(new byte[] {1, 2, 3, 4}, "en");
                     AudioData frame1 = makePcmS16le(new byte[] {5, 6, 7, 8}, null);
                     AudioData frame2 = makePcmS16le(new byte[] {9, 10, 11, 12}, "fr")) {

                    input.add(frame0);
                    input.add(frame1);

                    try {
                        input.send(frame2);
                        fail("expected send() while staged inputs are pending to throw RebarException");
                    } catch (RebarException error) {
                        assertEquals(ErrorCode.INPUT_PENDING, error.error().code());
                    }

                    pipelineSession.submit();
                    input.send(frame2);
                }

                pipelineSession.finishInputs();

                for (int i = 0; i < 3; ++i) {
                    ReceiveResult<AudioData> received = tee0Tap.receive();
                    assertEquals(ReceiveStatus.SUCCESS, received.status());
                    assertNotNull(received.item());
                    try (AudioData data = received.item().data()) {
                        assertEquals(true, received.item().isFinal());
                        if (i == 0) assertAudioEquals(makePcmS16le(new byte[] {1, 2, 3, 4}, "en"), data);
                        if (i == 1) assertAudioEquals(makePcmS16le(new byte[] {5, 6, 7, 8}, null), data);
                        if (i == 2) assertAudioEquals(makePcmS16le(new byte[] {9, 10, 11, 12}, "fr"), data);
                    }
                }

                for (int i = 0; i < 3; ++i) {
                    ReceiveResult<AudioData> received = tee1Tap.receive();
                    assertEquals(ReceiveStatus.SUCCESS, received.status());
                    assertNotNull(received.item());
                    try (AudioData data = received.item().data()) {
                        assertEquals(true, received.item().isFinal());
                        if (i == 0) assertAudioEquals(makePcmS16le(new byte[] {1, 2, 3, 4}, "en"), data);
                        if (i == 1) assertAudioEquals(makePcmS16le(new byte[] {5, 6, 7, 8}, null), data);
                        if (i == 2) assertAudioEquals(makePcmS16le(new byte[] {9, 10, 11, 12}, "fr"), data);
                    }
                }

                for (int i = 0; i < 3; ++i) {
                    ReceiveResult<AudioData> out = tee2Out.receive();
                    assertEquals(ReceiveStatus.SUCCESS, out.status());
                    assertNotNull(out.item());
                    try (AudioData data = out.item().data()) {
                        assertEquals(true, out.item().isFinal());
                        if (i == 0) assertAudioEquals(makePcmS16le(new byte[] {1, 2, 3, 4}, "en"), data);
                        if (i == 1) assertAudioEquals(makePcmS16le(new byte[] {5, 6, 7, 8}, null), data);
                        if (i == 2) assertAudioEquals(makePcmS16le(new byte[] {9, 10, 11, 12}, "fr"), data);
                    }
                }

                for (int i = 0; i < 3; ++i) {
                    ReceiveResult<AudioData> out = tee2Tap.receive();
                    assertEquals(ReceiveStatus.SUCCESS, out.status());
                    assertNotNull(out.item());
                    try (AudioData data = out.item().data()) {
                        assertEquals(true, out.item().isFinal());
                        if (i == 0) assertAudioEquals(makePcmS16le(new byte[] {1, 2, 3, 4}, "en"), data);
                        if (i == 1) assertAudioEquals(makePcmS16le(new byte[] {5, 6, 7, 8}, null), data);
                        if (i == 2) assertAudioEquals(makePcmS16le(new byte[] {9, 10, 11, 12}, "fr"), data);
                    }
                }

                ReceiveResult<AudioData> end0Tap = tee0Tap.receive();
                assertEquals(ReceiveStatus.SUCCESS, end0Tap.status());
                assertNull(end0Tap.item());

                ReceiveResult<AudioData> end1Tap = tee1Tap.receive();
                assertEquals(ReceiveStatus.SUCCESS, end1Tap.status());
                assertNull(end1Tap.item());

                ReceiveResult<AudioData> end2Out = tee2Out.receive();
                assertEquals(ReceiveStatus.SUCCESS, end2Out.status());
                assertNull(end2Out.item());

                ReceiveResult<AudioData> end2Tap = tee2Tap.receive();
                assertEquals(ReceiveStatus.SUCCESS, end2Tap.status());
                assertNull(end2Tap.item());
            }
        }
    }

    @Test
    public void testAudioLanguageRoundTripUnicode() {
        try (RebarNode tee = Tee.createNode(1)) {

            try (RebarPipelineSession pipelineSession = tee.createPipelineSession();
                 RebarNodeSession teeSession = pipelineSession.getNodeSession(tee);
                 AudioInput input = teeSession.getAudioInput();
                 AudioOutput out = teeSession.getAudioOutput(0)) {

                assertNotNull(input);
                assertNotNull(out);

                String loneHighSurrogate = new String(new char[] {'\uD83D'});
                String loneLowSurrogate = new String(new char[] {'\uDE00'});

                String[] inputs = new String[] {
                        null,
                        "",
                        "ASCII",
                        "Grüße",
                        "こんにちは",
                        "😀",
                        "a\u0000b",
                        "mix-\u0000-😀-€",
                        loneHighSurrogate,
                        loneLowSurrogate,
                };

                String[] expected = new String[] {
                        null,
                        "",
                        "ASCII",
                        "Grüße",
                        "こんにちは",
                        "😀",
                        "a\u0000b",
                        "mix-\u0000-😀-€",
                        "\uFFFD",
                        "\uFFFD",
                };

                for (int i = 0; i < inputs.length; ++i) {
                    byte[] bytes = new byte[] {(byte) i, (byte) (i + 1), (byte) (i + 2), (byte) (i + 3)};
                    try (AudioData frame = makePcmS16le(bytes, inputs[i])) {
                        input.send(frame);
                    }
                }

                pipelineSession.finishInputs();

                for (int i = 0; i < expected.length; ++i) {
                    ReceiveResult<AudioData> received = out.receive();
                    assertEquals(ReceiveStatus.SUCCESS, received.status());
                    assertNotNull(received.item());
                    try (AudioData data = received.item().data()) {
                        assertEquals(true, received.item().isFinal());
                        assertEquals(expected[i], data.language());
                    }
                }

                ReceiveResult<AudioData> end = out.receive();
                assertEquals(ReceiveStatus.SUCCESS, end.status());
                assertNull(end.item());
            }
        }
    }
}
