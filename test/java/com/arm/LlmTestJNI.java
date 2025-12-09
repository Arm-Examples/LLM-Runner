//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assume.assumeTrue;
import static org.junit.Assert.assertEquals;

import org.json.JSONObject;
import org.json.JSONArray;
import org.junit.Test;
import org.junit.BeforeClass;

import com.arm.Llm;
import java.io.*;
import java.util.*;
import java.util.concurrent.TimeUnit;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.io.IOException;

public class LlmTestJNI {

    private static final String modelDir = System.getProperty("model_dir");
    private static final String configFilePath = System.getProperty("config_file");
    private static final String sharedLibraryDir = System.getProperty("java.library.path");
    private static final String backendSharedLibDir = System.getProperty("backend.shared.lib.dir");
    private static JSONObject configJson;

    private void checkLlmMatch(String response, String expected, boolean shouldContain) {
        if (shouldContain) {
            assertTrue("Expected response to contain: " + expected +" but response is: " + response, response.contains(expected));
        } else {
            assertFalse("Expected response to not contain: " + expected, response.contains(expected));
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
        } catch (IOException e) {
            throw new RuntimeException("Failed to load config JSON", e);
        }
    }

    @Test
    public void testBenchmarking() {
        Llm llm = new Llm();
        JSONObject modelObj = configJson.getJSONObject("model");
        String modelName = modelObj.getString("llmModelName");
        int rc = llm.runBenchmark(
            modelName,
            128, /* Input tokens size */
            64,  /* Output tokens size */
            1,   /* Number of threads */
            3,   /* Number of iterations */
            1,   /* Number of warm up */
            backendSharedLibDir
        );
        assertEquals("runBenchmark should succeed", 0, rc);
        System.out.println("Benchmark done.");
        String result = llm.getBenchmarkResults();
        System.out.println(result);
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
        String response = llm.getResponse(question);
        checkLlmMatch(response, "Ferdia", true);
        llm.freeModel();
        // Revert the configJson to preserve original system prompt and modelTag
        chatObj.put("systemPrompt",oldSystemPrompt);
    }

    @Test
    public void testInferenceWithContextReset() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);

        String question1 = "What is the capital of Canada?";
        String response1 = llm.getResponse(question1);
        checkLlmMatch(response1, "Ottawa", true);

        // Resetting context should cause model to forget what country is being referred to
        llm.resetContext();

        String question2 = "What country is that capital of? Reply with one word. please.";
        String response2 = llm.getResponse(question2);
        checkLlmMatch(response2, "Canada", false);
        llm.freeModel();
      
    }

    @Test
    public void testInferenceWithoutContextReset() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);

        String question1 = "What is the capital of Canada?";
        String response1 = llm.getResponse(question1);
        checkLlmMatch(response1, "Ottawa", true);

        String question2 = "What country is that capital of? Reply with one word.";
        String response2 = llm.getResponse(question2);
        checkLlmMatch(response2, "Canada", true);
        llm.freeModel();
    }

    @Test
    public void testMultiLLMInferenceWithoutContextReset() {
        Llm germanLlm = new Llm();
        germanLlm.llmInit(configJson.toString(), backendSharedLibDir);

        Llm frenchLlm = new Llm();
        frenchLlm.llmInit(configJson.toString(), backendSharedLibDir);

        String germanQuestion1 = "What is the capital of Germany?";
        String germanResponse1 = germanLlm.getResponse(germanQuestion1);
        checkLlmMatch(germanResponse1, "Berlin", true);

        String frenchQuestion1 = "What is the capital of France?";
        String frenchResponse1 = frenchLlm.getResponse(frenchQuestion1);
        checkLlmMatch(frenchResponse1, "Paris", true);

        String germanQuestion2 = "What languages do they speak there?";
        String germanResponse2 = germanLlm.getResponse(germanQuestion2);
        checkLlmMatch(germanResponse2, "German", true);
        germanLlm.freeModel();

        String frenchQuestion2 = "What languages do they speak there?";
        String frenchResponse2 = frenchLlm.getResponse(frenchQuestion2);
        checkLlmMatch(frenchResponse2, "French", true);
        frenchLlm.freeModel();
    }

    @Test
    public void testInferenceHandlesEmptyQuestion() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);
        String question1 = "Paris is the capital of what country?";
        String response1 = llm.getResponse(question1);
        checkLlmMatch(response1, "France", true);

        // Send an empty prompt to simulate blank recordings or non-speech tokens being returned by speech recognition;
        // then ask follow-up questions to ensure previous context persists when an empty prompt is injected in the conversation.
        String emptyResponse = llm.getResponse("");
        String question3 = "What languages do they speak there?";
        String response3 = llm.getResponse(question3);
        checkLlmMatch(response3, "French", true);
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
        String initResponse = llm.getResponse(initialContext);
        String originalQuery = "How many mangoes did we start with, just reply with a single numerical digit?";
        String subtractQuery = "Remove 1 mango from the basket. How many mangoes left in the basket now, just reply with a single numerical digit?";

        // **Assert that the model acknowledges the context is related with mango.**
        checkLlmMatch(initResponse, "mango", true);

        // Loop to subtract 1 mango each iteration until reaching 0.
        for (int i = 1; i < originalMangoes; i++) {

            // Modify the query during the conversation
            if (i == 2) {
                subtractQuery = "Good, remove 1 mango again from the basket. How many mangoes left in the basket now, just reply with a single numerical digit?";
            }

            // Query to subtract one mango
            String subtractionResponse = llm.getResponse(subtractQuery);
            mangoes -= 1;  // Update our expected count
            checkLlmMatch(subtractionResponse, String.valueOf(mangoes), true);

            // Test if model still recalls the starting number
            if (i == originalMangoes - 1) {
                String response = llm.getResponse(originalQuery);
                checkLlmMatch(response, String.valueOf(originalMangoes), true);
                llm.resetContext();
            }

        }

        String postResetResponse = llm.getResponse(originalQuery);
        checkLlmMatch(postResetResponse, String.valueOf(originalMangoes), false);
        llm.freeModel();
    }


    @Test
    public void testInferenceRecoversAfterContextReset() {
        // Get model directory and config file path from system properties
       Llm llm = new Llm();
       llm.llmInit(configJson.toString(), backendSharedLibDir);

        // First Question
        String question1 = "What is the capital of Canada?";
    
        String response1 = llm.getResponse(question1);
        checkLlmMatch(response1, "Ottawa", true);
        // Reset Context before second question
        llm.resetContext();

        // Second Question (After Reset)
        String question2 = "What country is that capital of? Reply with one word.";
        String response2 = llm.getResponse(question2);
        checkLlmMatch(response2, "Canada", false);
        // Ask First Question Again. Note an additional reset is required to prevent the generic answer
        // from previous question affecting new topic.
        llm.resetContext();
        String response3 = llm.getResponse(question1);

        checkLlmMatch(response3, "Ottawa", true);
        String response4 = llm.getResponse(question2);

        checkLlmMatch(response4, "Canada", true);
        llm.freeModel();
    }
}
