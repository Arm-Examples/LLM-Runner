//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

package com.arm;

import static org.junit.Assert.*;
import static org.junit.Assume.assumeTrue;

import org.junit.Test;
import org.junit.BeforeClass;

import com.arm.llm.Llama;
import com.arm.llm.LlamaConfig;

import java.io.*;
import java.util.*;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Flow;
import java.util.concurrent.TimeUnit;


public class LlamaTestJNI {
    private static final String modelDir = System.getProperty("model_dir");
    private static final String configFilePath = System.getProperty("config_file");
    private static final Map<String, String> variables = new HashMap<>();
    private static final String LLAMA_MODEL_NAME = "model.gguf";
    private static final int numThreads = 4;

    // Timeout for subscriber latch await in seconds
    private static final long LATCH_TIMEOUT_SECONDS = 5;

    private static String modelTag = "";
    private static String modelPath = "";
    private static String llmPrefix = "";
    private static List<String> stopWords = new ArrayList<>();

    /**
     * Instead of matching the actual response to expected response,
     * check whether the response contains the salient parts of expected response.
     * Pass true to check match and false to assert absence of salient parts for negative tests.
     */
    private static void checkLlamaMatch(String response, String expectedResponse, boolean checkMatch) {
        boolean matches = response.contains(expectedResponse);
        if (!matches) {
            System.out.println("Response mismatch: response={" + response + "} expected={" + expectedResponse + "}");
        }
        if (checkMatch) {
            assertTrue(matches);
        } else {
            assertFalse(matches);
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
                if (parts[0].trim().equals("stopWordsDefault")) {
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
        modelTag = variables.get("modelTagDefault");
        llmPrefix = variables.get("llmPrefixDefault");
        modelPath = modelDir + "/" + LLAMA_MODEL_NAME;
    }

    /**
     * A test implementation of Flow.Subscriber<String> that collects tokens from asynchronous operations.
     * It accumulates received tokens in a list and uses a CountDownLatch to signal when the end-of-stream
     * token ("<eos>") is received, allowing tests to wait for completion.
     */
    static class TestSubscriber implements Flow.Subscriber<String> {

        private Flow.Subscription subscription;
        private final List<String> receivedTokens = new ArrayList<>();
        // Latch to signal when <eos> has been received.
        private final CountDownLatch latch = new CountDownLatch(1);
        @Override
        public void onSubscribe(Flow.Subscription subscription) {
            this.subscription = subscription;
            // Request an unlimited number of tokens.
            subscription.request(Long.MAX_VALUE);
        }
        @Override
        public void onNext(String token) {
            receivedTokens.add(token);
            // If the token indicates end-of-stream, count down the latch.
            if ("<eos>".equals(token)) {
                latch.countDown();
            }
        }
        @Override
        public void onError(Throwable throwable) {
            // In case of error, count down the latch so the test can proceed.
            latch.countDown();
        }
        @Override
        public void onComplete() {
            latch.countDown();
        }
        public List<String> getReceivedTokens() {
            return receivedTokens;
        }
        public boolean await(long timeout, TimeUnit unit) throws InterruptedException {
            return latch.await(timeout, unit);
        }
    }

    @Test
    public void testAsyncPublishing() throws Exception {
        // Create and initialize the Llama instance with test config using global variables.
        Llama llama = new Llama();
        LlamaConfig llamaConfig = new LlamaConfig(modelTag, stopWords, modelPath, llmPrefix, numThreads);
        llama.llmInit(llamaConfig);
        // Set up our test subscriber.
        TestSubscriber subscriber = new TestSubscriber();
        llama.setSubscriber(subscriber);
        llama.sendAsync("what is 2 + 2");
        // Wait up to LATCH_TIMEOUT_SECONDS seconds for the subscriber to receive the <eos> token.
        boolean completed = subscriber.await(LATCH_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        assertTrue("Subscriber received <eos> token within timeout", completed);
        // Retrieve and check the received tokens.
        List<String> tokens = subscriber.getReceivedTokens();
        assertEquals("Last token should be <eos>", "<eos>", tokens.get(tokens.size() - 1));
        StringBuilder tokenString = new StringBuilder();
        for (String token : tokens) {
            tokenString.append(token);
        }
        // Check that the tokens contain the number "4".
        assertTrue("Tokens should contain the number 4", tokenString.toString().contains("4"));
        // Clean up the model resources.
        llama.freeModel();
    }

    @Test
    public void testAsyncInferenceWithoutContextReset() throws Exception {
        // Create and initialize the Llama instance with global config.
        LlamaConfig llamaConfig = new LlamaConfig(modelTag, stopWords, modelPath, llmPrefix, numThreads);
        Llama llama = new Llama();
        llama.llmInit(llamaConfig);
        TestSubscriber subscriber1 = new TestSubscriber();
        llama.setSubscriber(subscriber1);
        String question1 = "What is the capital of Morocco?";
        llama.sendAsync(question1);
        // Wait up to LATCH_TIMEOUT_SECONDS seconds for the subscriber to receive the <eos> token.
        boolean completed1 = subscriber1.await(LATCH_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        assertTrue("Subscriber received <eos> token within timeout for question1", completed1);
        // Retrieve and check the received tokens.
        List<String> tokens1 = subscriber1.getReceivedTokens();
        StringBuilder tokenString1 = new StringBuilder();
        for (String token : tokens1) {
            tokenString1.append(token);
        }
        String response1 = tokenString1.toString();
        checkLlamaMatch(response1, "Rabat", true);
        TestSubscriber subscriber2 = new TestSubscriber();
        llama.setSubscriber(subscriber2);
        String question2 = "What languages do they speak there?";
        llama.sendAsync(question2);
        boolean completed2 = subscriber2.await(LATCH_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        assertTrue("Subscriber received <eos> token within timeout for question2", completed2);
        List<String> tokens2 = subscriber2.getReceivedTokens();
        StringBuilder tokenString2 = new StringBuilder();
        for (String token : tokens2) {
            tokenString2.append(token);
        }
        String response2 = tokenString2.toString();
        checkLlamaMatch(response2, "Arabic", true);
        // Clean up the model resources.
        llama.freeModel();
    }

    @Test
    public void testAsyncInferenceRecoversAfterContextReset() throws Exception {
        LlamaConfig llamaConfig = new LlamaConfig(modelTag, stopWords, modelPath, llmPrefix, numThreads);
        Llama llama = new Llama();
        llama.llmInit(llamaConfig);
        String question1 = "What is the capital of Morocco?";
        TestSubscriber subscriber1 = new TestSubscriber();
        llama.setSubscriber(subscriber1);
        llama.sendAsync(question1);
        boolean completed1 = subscriber1.await(LATCH_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        assertTrue("Subscriber should receive <eos> token for question1", completed1);
        List<String> tokens1 = subscriber1.getReceivedTokens();
        StringBuilder tokenString1 = new StringBuilder();
        for (String token : tokens1) {
            tokenString1.append(token);
        }
        String response1 = tokenString1.toString();
        checkLlamaMatch(response1, "Rabat", true);
        // Reset context before the next question.
        llama.resetContext();
        String question2 = "What languages do they speak there?";
        TestSubscriber subscriber2 = new TestSubscriber();
        llama.setSubscriber(subscriber2);
        llama.sendAsync(question2);
        boolean completed2 = subscriber2.await(LATCH_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        assertTrue("Subscriber should receive <eos> token for question2", completed2);
        List<String> tokens2 = subscriber2.getReceivedTokens();
        StringBuilder tokenString2 = new StringBuilder();
        for (String token : tokens2) {
            tokenString2.append(token);
        }
        String response2 = tokenString2.toString();
        checkLlamaMatch(response2, "Arabic", false);
        llama.resetContext();
        TestSubscriber subscriber3 = new TestSubscriber();
        llama.setSubscriber(subscriber3);
        llama.sendAsync(question1);
        boolean completed3 = subscriber3.await(LATCH_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        assertTrue("Subscriber should receive <eos> token for question3", completed3);
        List<String> tokens3 = subscriber3.getReceivedTokens();
        StringBuilder tokenString3 = new StringBuilder();
        for (String token : tokens3) {
            tokenString3.append(token);
        }
        String response3 = tokenString3.toString();
        checkLlamaMatch(response3, "Rabat", true);
        // Fourth Question: Ask second question again.
        TestSubscriber subscriber4 = new TestSubscriber();
        llama.setSubscriber(subscriber4);
        llama.sendAsync(question2);
        boolean completed4 = subscriber4.await(LATCH_TIMEOUT_SECONDS, TimeUnit.SECONDS);
        assertTrue("Subscriber should receive <eos> token for question4", completed4);
        List<String> tokens4 = subscriber4.getReceivedTokens();
        StringBuilder tokenString4 = new StringBuilder();
        for (String token : tokens4) {
            tokenString4.append(token);
        }
        String response4 = tokenString4.toString();
        checkLlamaMatch(response4, "Arabic", true);
        checkLlamaMatch(response4, "French", true);
        // Free model resources.
        llama.freeModel();
    }


    @Test
    public void testConfigLoading() {
        LlamaConfig llamaConfig = new LlamaConfig(modelTag, stopWords, modelPath, llmPrefix, numThreads);
        assertTrue("Model tag is not empty", !llamaConfig.getModelTag().isEmpty());
        assertTrue("LLM prefix is not empty", !llamaConfig.getLlmPrefix().isEmpty());
        assertTrue("Stop words list is not empty", !llamaConfig.getStopWords().isEmpty());
    }

    @Test
    public void testLlmPrefixSetting() {
        LlamaConfig llamaConfig = new LlamaConfig(modelTag, stopWords, modelPath, llmPrefix, numThreads);
        Llama llama = new Llama();
        llama.llmInit(llamaConfig);

        String newModelTag = ("Ferdia");
        String newPrefix = "Transcript of a dialog, where the User interacts with an AI Assistant named " + newModelTag +
                ". " + newModelTag +
                " is helpful, polite, honest, good at writing and answers honestly with a maximum of two sentences. User:";

        llama.setLlmModelTag(newModelTag);
        llama.setLlmPrefix(newPrefix);

        String question = "What is your name?";
        String response = llama.send(question);
        checkLlamaMatch(response, "Ferdia", true);
        llama.freeModel();
    }

    @Test
    public void testInferenceWithContextReset() {
        LlamaConfig llamaConfig = new LlamaConfig(modelTag, stopWords, modelPath, llmPrefix, numThreads);
        Llama llama = new Llama();
        llama.llmInit(llamaConfig);

        String question1 = "What is the capital of Morocco?";
        String response1 = llama.send(question1);
        checkLlamaMatch(response1, "Rabat", true);

        // Resetting context should cause model to forget what country is being referred to
        llama.resetContext();

        String question2 = "What languages do they speak there?";
        String response2 = llama.send(question2);
        checkLlamaMatch(response2, "Arabic", false);

        llama.freeModel();
    }

    @Test
    public void testInferenceWithoutContextReset() {
        LlamaConfig llamaConfig = new LlamaConfig(modelTag, stopWords, modelPath, llmPrefix, numThreads);
        Llama llama = new Llama();
        llama.llmInit(llamaConfig);

        String question1 = "What is the capital of Morocco?";
        String response1 = llama.send(question1);
        checkLlamaMatch(response1, "Rabat", true);

        String question2 = "What languages do they speak there?";
        String response2 = llama.send(question2);
        checkLlamaMatch(response2, "Arabic", true);

        llama.freeModel();
    }

    @Test
    public void testInferenceHandlesEmptyQuestion() {
        LlamaConfig llamaConfig = new LlamaConfig(modelTag, stopWords, modelPath, llmPrefix, numThreads);
        Llama llama = new Llama();
        llama.llmInit(llamaConfig);

        String question1 = "What is the capital of Morocco?";
        String response1 = llama.send(question1);
        checkLlamaMatch(response1, "Rabat", true);

        // Send an empty prompt to simulate blank recordings or non-speech tokens being returned by speech recognition;
        // then ask follow-up questions to ensure previous context persists when an empty prompt is injected in the conversation.
        String emptyResponse = llama.send(""); // ToDo may revisit this to add an expected answer

        String question2 = "What languages do they speak there?";
        String response2 = llama.send(question2);
        checkLlamaMatch(response2, "Arabic", true);
        checkLlamaMatch(response2, "French", true);

        llama.freeModel();
    }

    @Test
    public void testMangoSubtractionLongConversation() {

        LlamaConfig llamaConfig = new LlamaConfig(modelTag, stopWords, modelPath, llmPrefix, numThreads);
        Llama llama = new Llama();
        llama.llmInit(llamaConfig);

        // 35 was determined to be upper limit for storing context but to avoid excessively long test runtime we cap at 20
        int originalMangoes = 5;
        int mangoes = originalMangoes;

        // Set the initial ground truth in the conversation.
        String initialContext = "There are " + originalMangoes + " mangoes.";
        String initResponse = llama.send(initialContext);
        String originalQuery = "How many mangoes were there originally?";
        String subtractQuery = "Subtract 1 mango.";

        // **Assert that the model acknowledges the initial count of mangoes.**
        checkLlamaMatch(initResponse, String.valueOf(originalMangoes), true);

        // Loop to subtract 1 mango each iteration until reaching 0.
        for (int i = 1; i < originalMangoes; i++) {

            // Query to subtract one mango
            String subtractionResponse = llama.send(subtractQuery);
            mangoes -= 1;  // Update our expected count
            checkLlamaMatch(subtractionResponse, String.valueOf(mangoes), true);

            // Test if model still recalls the starting number
            if (i == originalMangoes - 1) {
                String response = llama.send(originalQuery);
                checkLlamaMatch(response, String.valueOf(originalMangoes), true);
                llama.resetContext();
            }

        }

        String postResetResponse = llama.send(originalQuery);
        checkLlamaMatch(postResetResponse, String.valueOf(originalMangoes), false);
        llama.freeModel();
    }

    @Test
    public void testInferenceRecoversAfterContextReset() {
        // Get model directory and config file path from system properties
        String modelDir = System.getProperty("model_dir");
        String configFilePath = System.getProperty("config_file");
        if (modelDir == null || configFilePath == null) {
            throw new RuntimeException("System properties for model_dir or config_file are not set!");
        }

        LlamaConfig llamaConfig = new LlamaConfig(modelTag, stopWords, modelPath, llmPrefix, numThreads);
        // Initialize Llama with the loaded config
        Llama llama = new Llama();
        llama.llmInit(llamaConfig);

        // First Question
        String question1 = "What is the capital of Morocco?";
        String response1 = llama.send(question1);
        checkLlamaMatch(response1, "Rabat", true);
        // Reset Context before second question
        llama.resetContext();

        // Second Question (After Reset)
        String question2 = "What languages do they speak there?";
        String response2 = llama.send(question2);
        checkLlamaMatch(response2, "Arabic", false);
        // Ask First Question Again. Note an additional reset is required to prevent the generic answer from previous question affecting new topic.
        llama.resetContext();
        String response3 = llama.send(question1);

        checkLlamaMatch(response3, "Rabat", true);
        String response4 = llama.send(question2);
        checkLlamaMatch(response4, "Arabic", true);
        checkLlamaMatch(response4, "French", true);

        // Free model after use
        llama.freeModel();
    }
}
