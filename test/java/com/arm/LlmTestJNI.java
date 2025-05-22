//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assume.assumeTrue;

import org.junit.Test;
import org.junit.BeforeClass;

import com.arm.Llm;
import com.arm.LlmConfig;

import java.io.*;
import java.util.*;

public class LlmTestJNI {
    private static final String modelDir = System.getProperty("model_dir");
    private static final String configFilePath = System.getProperty("config_file");
    private static final String userConfigFilePath = System.getProperty("user_config_file");
    private static final Map<String, String> variables = new HashMap<>();
    private static int numThreads = 4;
    private static String modelTag = "";
    private static String userTag = "";
    private static String modelPath = "";
    private static String llmPrefix = "";
    private static List<String> stopWords = new ArrayList<String>();

    /**
     * Instead of matching LLM's response to expected response,
     * check whether the response contains the salient parts of expected response.
     * Pass true to check match and false to assert absence of salient parts for negative tests.
     */
    private static void checkLlmMatch(String response, String expectedResponse, boolean checkMatch) {
        boolean matches = response.contains(expectedResponse);
        if (checkMatch) {
            assertTrue("Response mismatch: response={" + response + "} should contain={" + expectedResponse + "}", matches);
        } else {
            assertFalse("Response mismatch: response={" + response + "} shouldn't contain={" + expectedResponse + "}", matches);
        }
    }

    /**
     * Loads variables from the specified configuration file.
     *
     * @param filePath Path to the configuration file.
     * @throws IOException If an I/O error occurs.
     */
    private static void loadVariables(String filePath) throws IOException {
        try (BufferedReader br = new BufferedReader(new FileReader(filePath))) {
            String line;
            while ((line = br.readLine()) != null) {
                if (!line.contains("=")) continue;
                String[] parts = line.split("=", 2);
                if (parts[0].trim().equals("stopWords")) {
                    stopWords.clear(); // Ensure no duplicates on reloading
                    stopWords.addAll(Arrays.asList(parts[1].split(",")));
                } else {
                    variables.put(parts[0].trim(), parts[1].trim());
                }
            }
        } catch (FileNotFoundException e) {
            throw new IOException("Configuration file not found: " + filePath);
        } catch (Exception e) {
            throw new IOException("Error reading configuration file: " + e.getMessage());
        }
    }

    @BeforeClass
    public static void classSetup() throws IOException {
        if (modelDir == null) throw new RuntimeException("System property 'model_dir' is not set!");
        if (configFilePath == null)
            throw new RuntimeException("System property 'config_file' is not set!");

        loadVariables(configFilePath);
        modelTag = variables.get("modelTag");
        userTag = variables.getOrDefault("userTag","");
        llmPrefix = variables.get("llmPrefix");
        modelPath = modelDir + "/" + variables.get("llmModelName");
        loadVariables(userConfigFilePath);
        try{
            numThreads = Integer.valueOf(variables.getOrDefault("numThreads","4"));
         }
         catch(NumberFormatException e){
            System.out.println("Number of Threads parameter not found in UserConfiguration File");
            numThreads = 4;
         }

    }

    @Test
    public void testConfigLoading() {
        LlmConfig llmConfig = new LlmConfig(modelTag, stopWords, modelPath, llmPrefix,userTag,numThreads);
        assertTrue("Model tag is not empty", !llmConfig.getModelTag().isEmpty());
        assertTrue("LLM prefix is not empty", !llmConfig.getLlmPrefix().isEmpty());
        assertTrue("Stop words list is not empty", !llmConfig.getStopWords().isEmpty());
    }

    @Test
    public void testLlmPrefixSetting() {
        LlmConfig llmConfig = new LlmConfig(modelTag, stopWords, modelPath, llmPrefix,userTag);
        Llm llm = new Llm();
        llm.llmInit(llmConfig);

        String newModelTag = ("Ferdia");
        String newPrefix = "Transcript of a dialog, where the User interacts with an AI Assistant named " + newModelTag +
                ". " + newModelTag +
                " is helpful, polite, honest, good at writing and answers honestly with a maximum of two sentences. User:";

        llm.setLlmModelTag(newModelTag);
        llm.setLlmPrefix(newPrefix);

        String question = "What is your name?";
        String response = llm.send(question);
        checkLlmMatch(response, "Ferdia", true);
        llm.freeModel();
    }

    @Test
    public void testInferenceWithContextReset() {
       LlmConfig llmConfig = new LlmConfig(modelTag, stopWords, modelPath, llmPrefix,userTag,numThreads);
       Llm llm = new Llm();
       llm.llmInit(llmConfig);

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
        LlmConfig llmConfig = new LlmConfig(modelTag, stopWords, modelPath, llmPrefix,userTag,numThreads);
        Llm llm = new Llm();
        llm.llmInit(llmConfig);

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
        LlmConfig llmConfig = new LlmConfig(modelTag, stopWords, modelPath, llmPrefix,userTag,numThreads);
        Llm llm = new Llm();
        llm.llmInit(llmConfig);

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

        LlmConfig llmConfig = new LlmConfig(modelTag, stopWords, modelPath, llmPrefix,userTag,numThreads);
        Llm llm = new Llm();
        llm.llmInit(llmConfig);

        // 35 was determined to be upper limit for storing context but to avoid excessively long test runtime we cap at 20
        int originalMangoes = 5;
        int mangoes = originalMangoes;

        // Set the initial ground truth in the conversation.
        String initialContext = "There are " + originalMangoes + " mangoes in a basket.";
        String initResponse = llm.send(initialContext);
        String originalQuery = "How many mangoes did we start with? ";
        String subtractQuery = "Subtract 1 mango. How many mangoes are left now? ";

        // **Assert that the model acknowledges the initial count of mangoes.**
        checkLlmMatch(initResponse, String.valueOf(originalMangoes), true);

        // Loop to subtract 1 mango each iteration until reaching 0.
        for (int i = 1; i < originalMangoes; i++) {

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
        String modelDir = System.getProperty("model_dir");
        String configFilePath = System.getProperty("config_file");
        if (modelDir == null || configFilePath == null) {
            throw new RuntimeException("System properties for model_dir or config_file are not set!");
        }

        LlmConfig llmConfig = new LlmConfig(modelTag, stopWords, modelPath, llmPrefix,userTag,numThreads);
        Llm llm = new Llm();
        llm.llmInit(llmConfig);

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
        checkLlmMatch(response4, "French", true);

        // Free model after use
        llm.freeModel();
    }
}
