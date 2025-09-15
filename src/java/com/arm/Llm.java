//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm;

import java.util.concurrent.Flow;
import java.util.concurrent.SubmissionPublisher;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Llm class that extends the SubmissionPublisher
 */
public class Llm extends SubmissionPublisher<String> {

    private final AtomicBoolean evaluatedOnce = new AtomicBoolean(false);
    private static String eosToken = "<eos>";
    private String imagePath = "";
    private boolean imageUploaded = false;
     /**
      * @brief Maximum allowed input image dimension (in pixels).
      */
    public int maxInputImageDim = 128;

    static {
        try {
            System.loadLibrary("arm-llm-jni");
        } catch (UnsatisfiedLinkError e) {
            System.err.println("Llama: Failed to load library: arm-llm-jni");
            e.printStackTrace();
        }
    }

    /**
     * Create LLM native instance from config.
     *
     * @param configPathStr Path to config.json file.
     */
    public native void llmInit(String configPathStr);

    /**
     * @return Checks if the LLM impl supports Image input.
     */
    public native boolean supportsImageInput();

    /**
     * Free the LLM model (native).
     */
    private native void freeLlm();

    /**
     * @return Encoding rate in tokens/s.
     */
    public native float getEncodeRate();

    /**
     * @return Decoding rate in tokens/s.
     */
    public native float getDecodeRate();

    /**
     * Private method for resetting conversation history
     */
    public native void resetContext();

    /**
     * Reset timing information (native).
     */
    public native void resetTimings();

    /**
     * Method to encode the given text and image
     * @param text               the prompt to be encoded
     * @param pathToImage        path to the image to be encoded
     * @param isFirstMessage     boolean flag to signal if its the first message or not
     */
    private native void encode(String text, String pathToImage, boolean isFirstMessage);

    /**
     * Method to get Next Token once encoding is done.
     * This Method needs to be called in a loop while monitoring for Stop-Words.
     * @return next Token as String
     */
    private native String getNextToken();

    /**
     * @return Chat progress as percentage [0–100].
     */
    public native int getChatProgress();

    /**
     * Benchmark the model.
     *
     * @param nPrompts     Prompt length.
     * @param nEvalPrompts Number of generated tokens.
     * @param nMaxSeq      Sequence length.
     * @param nRep         Number of repetitions.
     * @return Benchmark results string.
     */
    public native String benchModel(int nPrompts, int nEvalPrompts, int nMaxSeq, int nRep);

    /**
     * @return Framework type as string.
     */
    public native String getFrameworkType();

    /**
     * @param subscriber Subscriber for LLM responses.
     */
    public void setSubscriber(Flow.Subscriber<String> subscriber) {
        this.subscribe(subscriber);
    }

    /**
     * Send a query asynchronously and stream results via SubmissionPublisher.
     * @param query  User query.
     * @param decode Whether to decode tokens as they arrive.
     */
    public void sendAsync(String query, boolean decode) {
        handleEncoding(query);
        evaluatedOnce.set(true);
        if (!decode) {
            return;
        }
        while (getChatProgress() < 100) {
            String token = getNextToken();
            if (eosToken.equals(token)) {
                this.submit(token); // signal end-of-stream
                break;
            }
            this.submit(token);
        }
    }

    /**
     * Send a query synchronously and return the response string.
     *
     * @param query  User query.
     * @param decode Whether to decode tokens.
     * @return Response string if decode = true, else empty string.
     */
    public String send(String query, boolean decode) {
        handleEncoding(query);
        evaluatedOnce.set(true);
        if (!decode) {
            return "";
        }
        StringBuilder response = new StringBuilder();
        while (getChatProgress() < 100) {
            String token = getNextToken();
            response.append(token);
            if (eosToken.equals(token)) {
                break;
            }
        }
        return response.toString();
    }

    /**
     * Internal helper for encoding queries depending on image state.
     *
     * @param query User query.
     */
    private void handleEncoding(String query) {
        if (!imageUploaded) {
            encode(query, "", !evaluatedOnce.get());
        } else {
            encode(query, imagePath, !evaluatedOnce.get());
            imageUploaded = false;
        }
    }

    /**
     * Free model from memory and close publisher.
     */
    public void freeModel() {
        freeLlm();
        this.close();
    }

    /**
     * Set image location for the next message.
     *
     * @param imagePath Path to image file.
     */
    public void setImageLocation(String imagePath) {
        this.imagePath = imagePath;
        this.imageUploaded = true;
    }
}
