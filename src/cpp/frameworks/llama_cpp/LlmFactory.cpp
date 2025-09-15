//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmFactory.hpp"
#include <algorithm>

std::unique_ptr<LLM::LLMImpl> LLMFactory::CreateLLMImpl(const LlmConfig &config) {
    std::vector<std::string> parsedInputModalities = config.GetInputModalities();

    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return s;
    };

    bool supportsVision = std::any_of(parsedInputModalities.begin(),
                                parsedInputModalities.end(),
                                [&](const std::string& s) {
                                    return toLower(s) == "image";
                                });
    if (supportsVision) {
        return std::make_unique<LlamaVisionImpl>();
    } else {
        return std::make_unique<LLM::LLMImpl>();
    }
}

