//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "Llm.hpp"

/**
 * @brief Abstract backend interface implemented by each framework.
 *
 * Concrete implementations live under src/cpp/frameworks/*.
 */
class LLM::LLMImpl : public LlmChat {
public:
    LLMImpl() = default;
    ~LLMImpl() override = default;

    LLMImpl(const LLMImpl&) = delete;
    LLMImpl& operator=(const LLMImpl&) = delete;
    LLMImpl(LLMImpl&&) noexcept = default;
    LLMImpl& operator=(LLMImpl&&) noexcept = default;

    virtual void LlmInit(const LlmConfig& config, std::string sharedLibraryPath = "") = 0;
    virtual void FreeLlm() = 0;
    virtual float GetEncodeTimings() = 0;
    virtual float GetDecodeTimings() = 0;
    virtual void ResetTimings() = 0;
    virtual std::string SystemInfo() = 0;
    virtual void ResetContext() = 0;
    virtual void Encode(LlmChat::Payload& payload) = 0;
    virtual std::string NextToken() = 0;
    virtual void Cancel() = 0;
    virtual size_t GetChatProgress() const = 0;
    virtual std::string GetFrameworkType() const = 0;
    virtual void StopGeneration() = 0;
    virtual std::vector<std::string> SupportedInputModalities() const = 0;
    virtual std::string GeneratePromptWithNumTokens(size_t numPromptTokens) = 0;
};
