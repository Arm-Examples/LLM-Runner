//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm;

import java.util.concurrent.Flow;
import java.util.concurrent.SubmissionPublisher;
import java.util.concurrent.atomic.AtomicBoolean;

import java.util.ArrayList;
import java.util.List;

/**
 * Llm class that extends the SubmissionPublisher
 */
public class Llm {

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
     * handle to the underlying native LLM instance.
     * Must be non-zero before any native operation is invoked.
     */
    private long nativeLlmHandle = 0L;

    /**
     * Sets the handle for the native Llm  instance.
     * @param handle to native llm (must be non-zero)
     */
    private void setNativeLlmHandle(long handle) {
        if (handle == 0L) {
            throw new IllegalArgumentException("nativeLlmHandle must be non-zero");
        }
        this.nativeLlmHandle = handle;
    }

    /**
     * Ensures the handle is set (non-zero) and returns it.
     * @return the validated, non-zero handle to the native Llm
     * @throws IllegalStateException if the handle is not set
     */
    private long getNativeLlmHandle() {
        if (nativeLlmHandle == 0L) {
            throw new IllegalStateException(
                    "nativeLlmHandle is not set. Call setnativeLlmHandle(...) before using this API 4."
            );
        }
        return nativeLlmHandle;
    }

    /**
     * Native method for initialising LLM model.
     * @param configPathStr path to config.json file as a string
     * @param sharedLibraryPath Path to shared library folder to load optional shared libs
     * @return native handle to the LLM instance
     */
    private native long llmInitJNI(String configPathStr, String sharedLibraryPath);

    /**
     * Native method to encode the given text and image
     * @param text               the prompt to be encoded
     * @param pathToImage        path to the image to be encoded
     * @param isFirstMessage     boolean flag to signal if its the first message or not
     * @param nativeLlmHandle    native handle to the LLM instance
     */
    private native void encodeJNI(String text, String pathToImage, boolean isFirstMessage, long nativeLlmHandle);

    /**
     * Native method to check if we supportsImageInput.
     * @param nativeLlmHandle handle to the native LLM instance
     * @return true if it support image encoding
     */
    private native boolean supportsImageInputJNI(long nativeLlmHandle);

    /**
     * Native method for freeing the native LLM model.
     * @param nativeLlmHandle native handle to the LLM instance
     */
    private native void freeLlmJNI(long nativeLlmHandle);

    /**
     * Native method for getting encode timing.
     * @param nativeLlmHandle native handle to the LLM instance
     * @return timings in tokens/s for encoding the prompt
     */
    private native float getEncodeRateJNI(long nativeLlmHandle);

    /**
     * Native method for getting decode timing.
     * @param nativeLlmHandle handle to the native LLM instance
     * @return timings in tokens/s for decoding the prompt
     */
    private native float getDecodeRateJNI(long nativeLlmHandle);

    /**
     * Native method for resetting conversation history/context.
     * @param nativeLlmHandle handle to the native LLM instance
     */
    private native void resetContextJNI(long nativeLlmHandle);

    /**
     * Native method for resetting timing information.
     * @param nativeLlmHandle handle to the native LLM instance
     */
    private native void resetTimingsJNI(long nativeLlmHandle);

    /**
     * Native method to encode the given text.
     * @param nativeLlmHandle handle to the native LLM instance
     * @param text the prompt to be encoded
     */
    private native void encodeJNI(long nativeLlmHandle, String text);

    /**
     * Native method to get the next token once encoding is done.
     * Should be called in a loop while monitoring for stop-words.
     * @param nativeLlmHandle handle to the native LLM instance
     * @return next token as String
     */
    private native String getNextTokenJNI(long nativeLlmHandle);

    /**
     * Method to produce next token, this API can be cancelled via cancel API
     * @param operationId can be used to return an error or check for user cancel operation requests
     * @param nativeLlmHandle handle to the native LLM instance
     * @return the next Token for Encoded Prompt
     */
    public native String getNextTokenCancellableJNI(long operationId, long nativeLlmHandle);
  
    /**
     * Native method to get chat progress in percentage.
     * @param nativeLlmHandle handle to the native LLM instance
     * @return chat progress as int (0–100)
     */
    private native int getChatProgressJNI(long nativeLlmHandle);

    /**
     * Native method the frameworkType
     * @param nativeLlmHandle handle to the native LLM instance
     * @return Framework type as string.
     */
    private native String getFrameworkTypeJNI(long nativeLlmHandle);

    /**
     * Method to check if we supportsImageInput.
     * @return true if model supports input image
     */
    public boolean supportsImageInput() {
        return supportsImageInputJNI(getNativeLlmHandle());
    }

    /**
     * Method for freeing LLM model.
     * Delegates to the native layer using the stored native handle.
     * @throws IllegalStateException if {@code nativeLlmHandle} is not set
     */
    public void freeLlm() {
        freeLlmJNI(getNativeLlmHandle());
        this.nativeLlmHandle = 0L;
    }

    /**
     * Public method for getting encode timing.
     * @return timings in tokens/s for encoding prompt
     * @throws IllegalStateException if {@code nativeLlmHandle} is not set
     */
    public float getEncodeRate() {
        return getEncodeRateJNI(getNativeLlmHandle());
    }

    /**
     * Public method for getting decode timing.
     * @return timings in tokens/s for decoding prompt
     * @throws IllegalStateException if {@code nativeLlmHandle} is not set
     */
    public float getDecodeRate() {
        return getDecodeRateJNI(getNativeLlmHandle());
    }

    /**
     * Method for resetting conversation history.
     * Calls the native implementation with the stored handle.
     * @throws IllegalStateException if {@code nativeLlmHandle} is not set
     */
    public void resetContext() {
        resetContextJNI(getNativeLlmHandle());
    }

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
    public void encode(String text, String pathToImage, boolean isFirstMessage) {
         encodeJNI(text, pathToImage, isFirstMessage, getNativeLlmHandle());
    }

    /**
     * Method to get Next Token once encoding is done.
     * This Method needs to be called in a loop while monitoring for Stop-Words.
     * @return next Token as String
     */
    public String getNextToken() {
        return getNextTokenJNI(getNativeLlmHandle());
    }

    /**
     * Method to produce next token
     * @param operationId can be used to return an error or check for user cancel operation requests
     * @return the next Token for Encoded Prompt
     */
    public String getNextTokenCancellable(long operationId) {
        return getNextTokenCancellableJNI(operationId, getNativeLlmHandle());
    }

    /**
     * Native Function to request the cancellation of a ongoing operation / functional call
     * @param operationId associated with operation / functional call
     * @param nativeLlmHandle handle to the native LLM instance
     */
    public native void cancelJNI(long operationId, long nativeLlmHandle);

    /**
     * Function to request the cancellation of a ongoing operation / functional call
     * @param operationId associated with operation / functional call
     */
    public void cancel(long operationId) {
        cancelJNI(operationId, getNativeLlmHandle());
    }

    /**
     * @return Chat progress as percentage [0–100].
     */
    public int getChatProgress() {
        return getChatProgressJNI(getNativeLlmHandle());
    }

    /**
     * Return the frameworkType
     * @return Framework type as string.
     */
    public native String getFrameworkType();


    /**
     * Submit a query synchronously.
     * @param query  User query.
     * @return void.
     */
    public void submit(String query) {

        if (query.length() > 0) {
            handleEncoding(query);
        }
    }

    /**
     * Method to create Llm cpp instance from params.
     * @param configPathStr path to config.json file as a string
     * @param sharedLibraryPath Path to shared library folder to load optional shared libs
     * @return handle to native LLM Instance
     */
    public void llmInit(String configPathStr, String sharedLibraryPath)
    {
        this.nativeLlmHandle = llmInitJNI(configPathStr, sharedLibraryPath);
    }

    /**
     * Submits query to LLM and returns response.
     * @param query  User query.
     * @return Response string .
     */
    public String getResponse(String query) {

        if (query.length()  <= 0) {
            return "";
        }

        submit(query);

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

    private void handleEncoding(String query) {
        if (!imageUploaded) {
            encode(query, "", true);
        } else {
            encode(query, imagePath, false);
            imageUploaded = false;
        }
    }

    /**
     * Free model from memory and close publisher.
     */
    public void freeModel() {
        freeLlm();
    }

    /**
     * Set image location for the next message.
     * @param imagePath Path to image file.
     */
    public void setImageLocation(String imagePath) {
        this.imagePath = imagePath;
        this.imageUploaded = true;
    }

    /**
     * Checks if it is a stop token (case insensetive)
     * @param token to check.
     * @return Bool, true if the token is a stop token 
    */
    public Boolean isStopToken(String token) {
        return token.equalsIgnoreCase(eosToken);
    }

    /**
     * Run the native LLM benchmark with the given parameters.
     *
     * This will execute the benchmark on the C++ side, including warmup
     * iterations and measured iterations. Timing results are cached and
     * can be retrieved via getBenchmarkResults().
     *
     * @param modelPath         Path to the model / config used by the LLM.
     * @param inputTokens       Number of input tokens for the prompt.
     * @param outputTokens      Number of tokens to generate in decode.
     * @param threads           Number of threads to use.
     * @param iterations        Number of measured iterations.
     * @param warmupIterations  Number of warmup iterations (ignored in stats).
     * @param sharedLibraryPath Path to directory with native shared libraries.
     * @return 0 on success, non-zero on failure.
     */
    public native int runBenchmark(
        String modelPath,
        int inputTokens,
        int outputTokens,
        int threads,
        int iterations,
        int warmupIterations,
        String sharedLibraryPath
    );

    /**
     * Get the last benchmark results as a formatted string.
     *
     * This does NOT run any benchmark; it simply returns the summary
     * from the most recent runBenchmark() call.
     *
     * @return Human-readable benchmark report, or an error message.
     */
    public native String getBenchmarkResults();

}
