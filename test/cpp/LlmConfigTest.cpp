//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//


#include "LlmImpl.hpp"
#include <sstream>
#include <list>
#include <cstring>

#include "catch2/catch_test_macros.hpp"

/**
 * Simple Test file for testing config related cases
 */

TEST_CASE("Test Multi-Modal config, with missing projection model")
{

    const std::string jsonString =
            "{\n"
            "  \"modelTag\": \"<|im_start|>assistant\\n\",\n"
            "  \"userTag\": \"<|im_start|>user\\n\",\n"
            "  \"endTag\" : \"<|im_end|>\\n\",\n"
            "  \"mediaTag\" : \"<__media__>\",\n"
            "  \"stopWords\": [\n"
            "    \"Orbita:\",\n"
            "    \"User:\",\n"
            "    \"AI:\",\n"
            "    \"<|user|>\",\n"
            "    \"Assistant:\",\n"
            "    \"user:\",\n"
            "    \"[end of text]\",\n"
            "    \"<|endoftext|>\",\n"
            "    \"<end_of_utterance>\",\n"
            "    \"model:\",\n"
            "    \"Question:\",\n"
            "    \"<|end|>\",\n"
            "    \"<|im_end|>\",\n"
            "    \"\\n\\n\",\n"
            "    \"Consider the following scenario:\\n\"\n"
            "  ],\n"
            "  \"llmModelName\": \"llama.cpp/mmModel.gguf\",\n"
            "  \"inputModalities\" : [\"text\", \"image\"],\n"
            "  \"outputModalities\" : [\"text\"],\n"
            "  \"llmPrefix\": \"<|im_start|>system\\nYou are a helpful and factual AI assistant named Orbita. Orbita answers with maximum of four sentences.\\n<|im_end|>\\n\",\n"
            "  \"numThreads\": 5,\n"
            "  \"maxInputImageDim\": 128\n"
            "}";

    try{
        LlmConfig config(jsonString);
    } catch (std::runtime_error& e)
    {
        CHECK(!strcmp(e.what(), "Missing required parameter: llmMmProjModelName"));
    }



}
