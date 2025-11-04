//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmFactory.hpp"
#include "Logger.hpp"

std::unique_ptr<LLM::LLMImpl> LLMFactory::CreateLLMImpl(const LlmConfig &config) {
    if (config.GetConfigBool("isVision")) {
        THROW_ERROR("Error, image input modality specified, but no supported by this LLMImpl");
    } else {
        return std::make_unique<LLM::LLMImpl>();
    }
}



