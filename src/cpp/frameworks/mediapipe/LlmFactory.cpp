//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmFactory.hpp"
#include <algorithm>
#include <stdexcept>


std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return s;
}

std::unique_ptr<LLM::LLMImpl> LLMFactory::CreateLLMImpl(const LlmConfig &config) {
    std::vector<std::string> parsedInputModalities = config.GetInputModalities();

    bool requestsVision = std::any_of(parsedInputModalities.begin(),
                                      parsedInputModalities.end(),
                                      [&](const std::string& s) {
                                          return toLower(s) == "image";
                                      });
    if (requestsVision) {
        throw std::runtime_error("Error, image input modality specified, but no supported by this LLMImpl");
    } else {
        return std::make_unique<LLM::LLMImpl>();
    }
}



