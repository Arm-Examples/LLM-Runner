//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assume.assumeTrue;
import static org.junit.Assert.assertEquals;

import org.json.JSONObject;
import org.junit.Test;
import org.junit.BeforeClass;

import com.arm.Llm;
import java.util.concurrent.TimeUnit;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

public class LlmTestJNI {

    private static final String modelDir = System.getProperty("model_dir");
    private static final String configFilePath = System.getProperty("config_file");
    private static final String sharedLibraryDir = System.getProperty("java.library.path");
    private static final String backendSharedLibDir = System.getProperty("backend.shared.lib.dir");
    private static JSONObject configJson;
    private static final boolean DEBUG = Boolean.parseBoolean(System.getProperty("llm.tests.debug", "false"));
    private static final String TRANSCRIPT_PATH = System.getProperty("llm.tests.transcript", "");

    private static String summarizeConfigForDebug() {
        try {
            JSONObject modelObj = configJson.getJSONObject("model");
            JSONObject runtimeObj = configJson.optJSONObject("runtime");
            JSONObject chatObj = configJson.optJSONObject("chat");

            String modelName = modelObj.optString("llmModelName", "<missing>");
            String projModel = modelObj.optString("projModelName", "");
            boolean isVision = modelObj.optBoolean("isVision", false);

            int contextSize = runtimeObj != null ? runtimeObj.optInt("contextSize", -1) : -1;
            int batchSize = runtimeObj != null ? runtimeObj.optInt("batchSize", -1) : -1;
            int numThreads = runtimeObj != null ? runtimeObj.optInt("numThreads", -1) : -1;

            boolean applyDefaultChatTemplate =
                chatObj != null && chatObj.has("applyDefaultChatTemplate") && chatObj.optBoolean("applyDefaultChatTemplate");

            return "model=" + modelName
                + (projModel.isEmpty() ? "" : (", projModel=" + projModel))
                + ", isVision=" + isVision
                + ", contextSize=" + contextSize
                + ", batchSize=" + batchSize
                + ", numThreads=" + numThreads
                + ", applyDefaultChatTemplate=" + applyDefaultChatTemplate;
        } catch (Exception e) {
            return "<failed to summarize config: " + e.getMessage() + ">";
        }
    }

    private static void appendTranscript(String text) {
        if (TRANSCRIPT_PATH == null || TRANSCRIPT_PATH.isEmpty()) {
            return;
        }
        try (FileWriter fw = new FileWriter(TRANSCRIPT_PATH, true);
             BufferedWriter bw = new BufferedWriter(fw)) {
            bw.write(text);
            if (!text.endsWith("\n")) {
                bw.newLine();
            }
        } catch (IOException e) {
            if (DEBUG) {
                System.err.println("Transcript write failed: " + e.getMessage());
            }
        }
    }

    private static void transcriptSeparator() {
        appendTranscript("----");
    }

    private static String getResponseOrFail(Llm llm, String prompt) {
        try {
            String response = llm.getResponse(prompt);
            if (DEBUG) {
                System.out.println("Prompt: " + prompt);
                System.out.println("Response: " + response);
            }
            if (TRANSCRIPT_PATH != null && !TRANSCRIPT_PATH.isEmpty()) {
                appendTranscript("Prompt: " + prompt);
                appendTranscript("Response: " + response);
            }
            return response;
        } catch (RuntimeException e) {
            String msg = "getResponse failed."
                + "\nprompt: " + prompt
                + "\nconfig: " + configFilePath
                + "\nmodelRoot: " + modelDir
                + "\nbackendSharedLibDir: " + backendSharedLibDir
                + "\nconfigSummary: " + summarizeConfigForDebug()
                + "\nerror: " + e.getMessage();
            transcriptSeparator();
            appendTranscript("getResponse failed");
            appendTranscript("prompt: " + prompt);
            appendTranscript("config: " + configFilePath);
            appendTranscript("modelRoot: " + modelDir);
            appendTranscript("backendSharedLibDir: " + backendSharedLibDir);
            appendTranscript("configSummary: " + summarizeConfigForDebug());
            appendTranscript("error: " + e.getMessage());
            throw new AssertionError(msg, e);
        }
    }

    private static void checkLlmMatch(String prompt, String response, String expected, boolean shouldContain) {
        if (shouldContain) {
            assertTrue(
                "Expected response to contain: " + expected
                    + "\nprompt: " + prompt
                    + "\nresponse: " + response
                    + "\nconfig: " + configFilePath
                    + "\nmodelRoot: " + modelDir
                    + "\nbackendSharedLibDir: " + backendSharedLibDir
                    + "\nconfigSummary: " + summarizeConfigForDebug(),
                response.contains(expected));
        } else {
            assertFalse(
                "Expected response to NOT contain: " + expected
                    + "\nprompt: " + prompt
                    + "\nresponse: " + response
                    + "\nconfig: " + configFilePath
                    + "\nmodelRoot: " + modelDir
                    + "\nbackendSharedLibDir: " + backendSharedLibDir
                    + "\nconfigSummary: " + summarizeConfigForDebug(),
                response.contains(expected));
        }
    }

    @BeforeClass
    public static void classSetup() {
        try {
            String jsonContent = new String(Files.readAllBytes(Paths.get(configFilePath)));
            configJson = new JSONObject(jsonContent);
            JSONObject modelObj = configJson.getJSONObject("model");
            String modelName = modelObj.getString("llmModelName");
            modelObj.put("llmModelName", modelDir + "/" + modelName);
            if (modelObj.has("projModelName") && !modelObj.isNull("projModelName")) {
                String projModelName = modelObj.getString("projModelName");
                if (!projModelName.isEmpty()) {
                    modelObj.put("projModelName", modelDir + "/" + projModelName);
                }   
            }
            if (DEBUG) {
                System.out.println("JNI test config loaded.");
                System.out.println("config: " + configFilePath);
                System.out.println("modelRoot: " + modelDir);
                System.out.println("backendSharedLibDir: " + backendSharedLibDir);
                System.out.println("configSummary: " + summarizeConfigForDebug());
            }
            if (TRANSCRIPT_PATH != null && !TRANSCRIPT_PATH.isEmpty()) {
                transcriptSeparator();
                appendTranscript("JNI test config loaded");
                appendTranscript("config: " + configFilePath);
                appendTranscript("modelRoot: " + modelDir);
                appendTranscript("backendSharedLibDir: " + backendSharedLibDir);
                appendTranscript("configSummary: " + summarizeConfigForDebug());
            }
        } catch (IOException e) {
            throw new RuntimeException("Failed to load config JSON", e);
        }
    }

    //@Test
    public void testBenchmarking() {
        Llm llm = new Llm();
        JSONObject modelObj = configJson.getJSONObject("model");
        String modelName = modelObj.getString("llmModelName");
        int resultCode = llm.runBenchmark(
            modelName,
            128, /* Input tokens size */
            64,  /* Output tokens size */
            512, /* Context size */
            1,   /* Number of threads */
            3,   /* Number of iterations */
            1,   /* Number of warm up */
            backendSharedLibDir
        );
        assertEquals("runBenchmark should succeed", 0, resultCode);
        System.out.println("Benchmark done.");
        String jsonResult = llm.getBenchmarkResultsJson();
        JSONObject benchmarkJson = new JSONObject(jsonResult);
        JSONObject overheadMetrics = new JSONObject();
        overheadMetrics.put("java_encode_total_ms", llm.getLastBenchmarkJavaEncodeTotalMs());
        overheadMetrics.put("core_cpp_encode_total_ms", llm.getLastBenchmarkCoreCppEncodeTotalMs());
        overheadMetrics.put("encode_overhead_ms", llm.getLastBenchmarkEncodeOverheadMs());
        overheadMetrics.put("java_decode_loop_total_ms", llm.getLastBenchmarkJavaDecodeLoopTotalMs());
        overheadMetrics.put("core_cpp_decode_total_ms", llm.getLastBenchmarkCoreCppDecodeTotalMs());
        overheadMetrics.put("decode_overhead_ms", llm.getLastBenchmarkDecodeOverheadMs());
        benchmarkJson.put("java_core_overhead_metrics", overheadMetrics);
        String configBaseName = Paths.get(configFilePath).getFileName().toString();
        int extIndex = configBaseName.lastIndexOf('.');
        if (extIndex > 0) {
            configBaseName = configBaseName.substring(0, extIndex);
        }
        String timestamp = LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyyMMdd_HHmmss"));
        Path buildDirPath = Paths.get(backendSharedLibDir).getParent();
        if (buildDirPath == null) {
            throw new RuntimeException("Unable to resolve build directory from backendSharedLibDir: " + backendSharedLibDir);
        }
        Path benchmarkResultsDir = buildDirPath.resolve("benchmark_results");
        String outputFileName = configBaseName + ".benchmark." + timestamp + ".json";
        Path outputPath = benchmarkResultsDir.resolve(outputFileName);
        try {
            Files.createDirectories(benchmarkResultsDir);
            Files.writeString(outputPath, benchmarkJson.toString(2));
        } catch (IOException e) {
            throw new RuntimeException("Failed to write benchmark JSON to " + outputPath, e);
        } finally {
            llm.freeBenchmark();
        }
    }

    @Test
    public void testSystemPrompt() {
        String newModelTag = "Ferdia";
        String newSystemPrompt = "You are a helpful and factual AI assistant named "+ newModelTag + ". " + newModelTag +  " answers with maximum of two sentences.";
        JSONObject chatObj = configJson.getJSONObject("chat");
        String oldSystemPrompt = chatObj.getString("systemPrompt");
        chatObj.put("systemPrompt",newSystemPrompt);
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);
        String question = "What is your name?";
        String response = getResponseOrFail(llm, question);
        checkLlmMatch(question, response, "Ferdia", true);
        llm.freeModel();
        // Revert the configJson to preserve original system prompt and modelTag
        chatObj.put("systemPrompt",oldSystemPrompt);
    }

    @Test
    public void testInferenceWithContextReset() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);

        String question1 = "What is the capital of Canada?";
        String response1 = getResponseOrFail(llm, question1);
        checkLlmMatch(question1, response1, "Ottawa", true);

        // Resetting context should cause model to forget what country is being referred to
        llm.resetContext();

        String question2 = "What country is that capital of? Reply with one word. please.";
        String response2 = getResponseOrFail(llm, question2);
        checkLlmMatch(question2, response2, "Canada", false);
        llm.freeModel();
      
    }

    @Test
    public void testInferenceWithoutContextReset() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);

        String question1 = "What is the capital of Canada?";
        String response1 = getResponseOrFail(llm, question1);
        checkLlmMatch(question1, response1, "Ottawa", true);

        String question2 = "What country is that capital of? Reply with one word.";
        String response2 = getResponseOrFail(llm, question2);
        checkLlmMatch(question2, response2, "Canada", true);
        llm.freeModel();
    }

    @Test
    public void testMultiLLMInferenceWithoutContextReset() {
        Llm germanLlm = new Llm();
        germanLlm.llmInit(configJson.toString(), backendSharedLibDir);

        Llm frenchLlm = new Llm();
        frenchLlm.llmInit(configJson.toString(), backendSharedLibDir);

        String germanQuestion1 = "What is the capital of Germany?";
        String germanResponse1 = getResponseOrFail(germanLlm, germanQuestion1);
        checkLlmMatch(germanQuestion1, germanResponse1, "Berlin", true);

        String frenchQuestion1 = "What is the capital of France?";
        String frenchResponse1 = getResponseOrFail(frenchLlm, frenchQuestion1);
        checkLlmMatch(frenchQuestion1, frenchResponse1, "Paris", true);

        String germanQuestion2 = "What languages do they speak there?";
        String germanResponse2 = getResponseOrFail(germanLlm, germanQuestion2);
        checkLlmMatch(germanQuestion2, germanResponse2, "German", true);
        germanLlm.freeModel();

        String frenchQuestion2 = "What languages do they speak there?";
        String frenchResponse2 = getResponseOrFail(frenchLlm, frenchQuestion2);
        checkLlmMatch(frenchQuestion2, frenchResponse2, "French", true);
        frenchLlm.freeModel();
    }

    @Test
    public void testInferenceHandlesEmptyQuestion() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);
        String question1 = "Paris is the capital of what country?";
        String response1 = getResponseOrFail(llm, question1);
        checkLlmMatch(question1, response1, "France", true);

        // Send an empty prompt to simulate blank recordings or non-speech tokens being returned by speech recognition;
        // then ask follow-up questions to ensure previous context persists when an empty prompt is injected in the conversation.
        String emptyResponse = llm.getResponse("");
        String question3 = "What languages do they speak there?";
        String response3 = getResponseOrFail(llm, question3);
        checkLlmMatch(question3, response3, "French", true);
        llm.freeModel();
    }

    //Disabling test, it is failing intermittently on multiple backends/models
    //@Test
    public void testMangoSubtractionLongConversation() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);

        int originalMangoes = 5;
        int mangoes = originalMangoes;

        // Set the initial ground truth in the conversation.
        String initialContext = "There are " + originalMangoes + " mangoes in a basket.";
        String initResponse = getResponseOrFail(llm, initialContext);
        String originalQuery = "How many mangoes did we start with, just reply with a single numerical digit?";
        String subtractQuery = "Remove 1 mango from the basket. How many mangoes left in the basket now, just reply with a single numerical digit?";

        // **Assert that the model acknowledges the context is related with mango.**
        checkLlmMatch(initialContext, initResponse, "mango", true);

        // Loop to subtract 1 mango each iteration until reaching 0.
        for (int i = 1; i < originalMangoes; i++) {

            // Modify the query during the conversation
            if (i == 2) {
                subtractQuery = "Good, remove 1 mango again from the basket. How many mangoes left in the basket now, just reply with a single numerical digit?";
            }

            // Query to subtract one mango
            String subtractionResponse = getResponseOrFail(llm, subtractQuery);
            mangoes -= 1;  // Update our expected count
            checkLlmMatch(subtractQuery, subtractionResponse, String.valueOf(mangoes), true);

            // Test if model still recalls the starting number
            if (i == originalMangoes - 1) {
                String response = getResponseOrFail(llm, originalQuery);
                checkLlmMatch(originalQuery, response, String.valueOf(originalMangoes), true);
                llm.resetContext();
            }

        }

        String postResetResponse = getResponseOrFail(llm, originalQuery);
        checkLlmMatch(originalQuery, postResetResponse, String.valueOf(originalMangoes), false);
        llm.freeModel();
    }


    @Test
    public void testInferenceRecoversAfterContextReset() {
        // Get model directory and config file path from system properties
       Llm llm = new Llm();
       llm.llmInit(configJson.toString(), backendSharedLibDir);

        // First Question
        String question1 = "What is the capital of Canada?";
    
        String response1 = getResponseOrFail(llm, question1);
        checkLlmMatch(question1, response1, "Ottawa", true);
        // Reset Context before second question
        llm.resetContext();

        // Second Question (After Reset)
        String question2 = "What country is that capital of? Reply with one word.";
        String response2 = getResponseOrFail(llm, question2);
        checkLlmMatch(question2, response2, "Canada", false);
        // Ask First Question Again. Note an additional reset is required to prevent the generic answer
        // from previous question affecting new topic.
        llm.resetContext();
        String response3 = getResponseOrFail(llm, question1);

        checkLlmMatch(question1, response3, "Ottawa", true);
        String response4 = getResponseOrFail(llm, question2);

        checkLlmMatch(question2, response4, "Canada", true);
        llm.freeModel();
    }
}
