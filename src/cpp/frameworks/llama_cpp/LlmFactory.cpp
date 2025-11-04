//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmFactory.hpp"

std::unique_ptr<LLM::LLMImpl> LLMFactory::CreateLLMImpl(const LlmConfig &config) {
    if (config.GetConfigBool("isVision")) {
        return std::make_unique<LlamaVisionImpl>();
    } else {
        return std::make_unique<LLM::LLMImpl>();
    }
}

