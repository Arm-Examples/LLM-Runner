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
        } catch (IOException e) {
            throw new RuntimeException("Failed to load config JSON", e);
        }
    }
    @Test
    public void testLlmPrefixSetting() {


        String newModelTag = "Ferdia:";
        String newPrefix = "Transcript of a dialog, where the User interacts with an AI Assistant named " + newModelTag +
                ". " + newModelTag +
                " is helpful, polite, honest, good at writing and answers honestly with a maximum of two sentences. User:";
        String oldTag = configJson.getString("modelTag");
        String oldPrefix = configJson.getString("llmPrefix");

        configJson.put("modelTag",newModelTag);
        configJson.put("llmPrefix",newPrefix);
        Llm llm = new Llm();
        llm.llmInit(configJson.toString());
        String question = "What is your name?";
        String response = llm.send(question);
        checkLlmMatch(response, "Ferdia", true);
        llm.freeModel();
        // Revert the configJson to preserve original prefix and modelTag
        configJson.put("modelTag",oldTag);
        configJson.put("llmPrefix",oldPrefix);

    }

    @Test
    public void testInferenceWithContextReset() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString());

        String question1 = "What is the capital of the country, Morocco?";
        String response1 = llm.send(question1);
        checkLlmMatch(response1, "Rabat", true);

        // Resetting context should cause model to forget what country is being referred to
        llm.resetContext();

        String question2 = "What languages do they speak there?";
        String response2 = llm.send(question2);
        checkLlmMatch(response2, "Arabic", false);
        llm.freeModel();
    }

    @Test
    public void testInferenceWithoutContextReset() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString());

        String question1 = "What is the capital of the country, Morocco?";
        String response1 = llm.send(question1);
        checkLlmMatch(response1, "Rabat", true);

        String question2 = "What languages do they speak there?";
        String response2 = llm.send(question2);
        checkLlmMatch(response2, "Arabic", true);
        llm.freeModel();
    }
    @Test
    public void testInferenceHandlesEmptyQuestion() {
        Llm llm = new Llm();
        llm.llmInit(configJson.toString());

        String question1 = "What is the capital of the country, Morocco?";
        String response1 = llm.send(question1);
        checkLlmMatch(response1, "Rabat", true);

        // Send an empty prompt to simulate blank recordings or non-speech tokens being returned by speech recognition;
        // then ask follow-up questions to ensure previous context persists when an empty prompt is injected in the conversation.
        String emptyResponse = llm.send("");

        checkLlmMatch(emptyResponse, "Rabat", true);

        String question2 = "What languages do they speak there?";
        String response2 = llm.send(question2);
        checkLlmMatch(response2, "Arabic", true);
        llm.freeModel();
    }

    @Test
    public void testMangoSubtractionLongConversation() {

       Llm llm = new Llm();
       llm.llmInit(configJson.toString());

        // 35 was determined to be upper limit for storing context but to avoid excessively long test runtime we cap at 20
        int originalMangoes = 5;
        int mangoes = originalMangoes;

        // Set the initial ground truth in the conversation.
        String initialContext = "There are " + originalMangoes + " mangoes in a basket.";
        String initResponse = llm.send(initialContext);
        String originalQuery = "How many mangoes did we start with?";
        String subtractQuery = "Remove 1 mango from the basket. How many mangoes left in the basket now?";

        // **Assert that the model acknowledges the initial count of mangoes.**
        checkLlmMatch(initResponse, String.valueOf(originalMangoes), true);

        // Loop to subtract 1 mango each iteration until reaching 0.
        for (int i = 1; i < originalMangoes; i++) {

            // Modify the query during the conversation
            if (i == 2) {
                subtractQuery = "Good, remove 1 mango again from the basket. How many mangoes left in the basket now?";
            }

            // Query to subtract one mango
            String subtractionResponse = llm.send(subtractQuery);
            mangoes -= 1;  // Update our expected count
            checkLlmMatch(subtractionResponse, String.valueOf(mangoes), true);

            // Test if model still recalls the starting number
            if (i == originalMangoes - 1) {
                String response = llm.send(originalQuery);
                checkLlmMatch(response, String.valueOf(originalMangoes), true);
                llm.resetContext();
            }

        }

        String postResetResponse = llm.send(originalQuery);

        checkLlmMatch(postResetResponse, String.valueOf(originalMangoes), false);
        llm.freeModel();
    }

    @Test
    public void testInferenceRecoversAfterContextReset() {
        // Get model directory and config file path from system properties
       Llm llm = new Llm();
       llm.llmInit(configJson.toString());

        // First Question
        String question1 = "What is the capital of the country, Morocco?";
        String response1 = llm.send(question1);
        checkLlmMatch(response1, "Rabat", true);
        // Reset Context before second question
        llm.resetContext();

        // Second Question (After Reset)
        String question2 = "What languages do they speak there?";
        String response2 = llm.send(question2);
        checkLlmMatch(response2, "Arabic", false);
        // Ask First Question Again. Note an additional reset is required to prevent the generic answer
        // from previous question affecting new topic.
        llm.resetContext();
        String response3 = llm.send(question1);

        checkLlmMatch(response3, "Rabat", true);
        String response4 = llm.send(question2);

        checkLlmMatch(response4, "Arabic", true);
        llm.freeModel();
    }

}
