//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmFactory.hpp"
#include "Logger.hpp"

std::unique_ptr<LLM::LLMImpl> LLMFactory::CreateLLMImpl(const LlmConfig& config)
{
    if (config.GetConfigBool(LlmConfig::ConfigParam::IsVision)) {
        THROW_ERROR("ExecuTorch backend skeleton only supports text models");
    }

    return std::make_unique<LLM::LLMImpl>();
}
