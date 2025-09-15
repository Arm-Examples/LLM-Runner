//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "catch.hpp"

#include "LlmImpl.hpp"
#include <sstream>
#include <list>

/**
 * Simple test to ensure we pick up the correct LLMImpl based on the modalities in the config
 */
TEST_CASE("Test Query")
{
    LlmConfig config{};
    std::vector<std::string> inputModalities;
    inputModalities.emplace_back("Text");
    std::vector<std::string> outputModalities;
    outputModalities.emplace_back("Text");
    config.SetInputModalities(inputModalities);
    config.SetOutputModalities(outputModalities);
    LLM textOnlyLlm(config);
    std::vector<std::string> modalities = textOnlyLlm.SupportedInputModalities();
    CHECK(modalities.size() == 1);

    if(config.GetInputModalities().size() == 2) {
        // Add image support
        inputModalities.emplace_back("Image");
        config.SetInputModalities(inputModalities);
        LLM vqaLLM(config);
        std::vector<std::string> vqaInputModalities = vqaLLM.SupportedInputModalities();
        CHECK(vqaInputModalities.size() == 2);
    }
}
