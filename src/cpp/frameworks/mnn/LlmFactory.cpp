//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmFactory.hpp"

std::unique_ptr<LLM::LLMImpl> LLMFactory::CreateLLMImpl(const LlmConfig &config) {

    // MNN supports both text and image input in same implementation file
    return std::make_unique<LLM::LLMImpl>();
}
