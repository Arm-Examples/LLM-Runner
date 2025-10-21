//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assume.assumeTrue;

import org.json.JSONObject;
import org.json.JSONArray;
import org.junit.Test;
import org.junit.BeforeClass;

import com.arm.Llm;
import java.io.*;
import java.util.*;
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
            configJson.put("llmModelName", modelDir + "/" + configJson.getString("llmModelName"));
            JSONArray inputModalities = configJson.getJSONArray("inputModalities");
            if (inputModalities.length() == 2) {
                configJson.put("llmMmProjModelName", modelDir + "/" + configJson.getString("llmMmProjModelName"));
            }
        } catch (IOException e) {
            throw new RuntimeException("Failed to load config JSON", e);
        }
    }

    @Test
    public void testSystemPrompt() {
        String newModelTag = "Ferdia";
        String newSystemPrompt = "You are a helpful and factual AI assistant named "+ newModelTag + ". " + newModelTag +  " answers with maximum of two sentences.";
        String oldSystemPrompt = configJson.getString("systemPrompt");

        configJson.put("systemPrompt",newSystemPrompt);
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);
        String question = "What is your name?";
        String response = llm.send(question, true);
        checkLlmMatch(response, "Ferdia", true);
        llm.freeModel();
        // Revert the configJson to preserve original system prompt and modelTag
        configJson.put("systemPrompt",oldSystemPrompt);
    }

    @Test
    public void testInferenceWithContextReset() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);

        String question1 = "What is the capital of Canada?";
        String response1 = llm.send(question1, true);
        checkLlmMatch(response1, "Ottawa", true);

        // Resetting context should cause model to forget what country is being referred to
        llm.resetContext();

        String question2 = "What languages do they speak there?";
        String response2 = llm.send(question2, true);
        checkLlmMatch(response2, "English", false);
        llm.freeModel();
    }

    @Test
    public void testInferenceWithoutContextReset() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);

        String question1 = "What is the capital of Canada?";
        String response1 = llm.send(question1, true);
        checkLlmMatch(response1, "Ottawa", true);

        String question2 = "What languages do they speak there?";
        String response2 = llm.send(question2, true);
        checkLlmMatch(response2, "English", true);
        llm.freeModel();
    }

    @Test
    public void testInferenceHandlesEmptyQuestion() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString(), backendSharedLibDir);

        String question1 = "What is the capital of Canada?";
        String response1 = llm.send(question1, true);
        checkLlmMatch(response1, "Ottawa", true);

        // Send an empty prompt to simulate blank recordings or non-speech tokens being returned by speech recognition;
        // then ask follow-up questions to ensure previous context persists when an empty prompt is injected in the conversation.
        String emptyResponse = llm.send("", true);

        String question2 = "What languages do they speak there?";
        String response2 = llm.send(question2, true);
        checkLlmMatch(response2, "English", true);
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
        String initResponse = llm.send(initialContext, true);
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
            String subtractionResponse = llm.send(subtractQuery, true);
            mangoes -= 1;  // Update our expected count
            checkLlmMatch(subtractionResponse, String.valueOf(mangoes), true);

            // Test if model still recalls the starting number
            if (i == originalMangoes - 1) {
                String response = llm.send(originalQuery, true);
                checkLlmMatch(response, String.valueOf(originalMangoes), true);
                llm.resetContext();
            }

        }

        String postResetResponse = llm.send(originalQuery, true);

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
        String response1 = llm.send(question1, true);
        checkLlmMatch(response1, "Ottawa", true);
        // Reset Context before second question
        llm.resetContext();

        // Second Question (After Reset)
        String question2 = "What languages do they speak there?";
        String response2 = llm.send(question2, true);
        checkLlmMatch(response2, "English", false);
        // Ask First Question Again. Note an additional reset is required to prevent the generic answer
        // from previous question affecting new topic.
        llm.resetContext();
        String response3 = llm.send(question1, true);

        checkLlmMatch(response3, "Ottawa", true);
        String response4 = llm.send(question2, true);

        checkLlmMatch(response4, "English", true);
        llm.freeModel();
    }
}