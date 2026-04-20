//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmImpl.hpp"

#include <utility>

#include "Logger.hpp"

void LLM::LLMImpl::LlmInit(const LlmConfig& config, std::string sharedLibraryPath)
{
    // Placeholder: record config only until the ExecuTorch runtime is wired in.
    m_config = config;
    m_modelPath = config.GetConfigString(LlmConfig::ConfigParam::LlmModelName);
    m_sharedLibraryPath = std::move(sharedLibraryPath);
    m_contextFilled = 0;
    m_cancelRequested = false;
    m_initialized = true;

    LOG_WARN("ExecuTorch backend initialized in skeleton mode; inference is not implemented yet");
    LOG_INF("ExecuTorch skeleton configured with model='%s'", m_modelPath.c_str());
}

void LLM::LLMImpl::FreeLlm()
{
    // Placeholder: runtime-owned resources will be released here once ExecuTorch is wired in.
    ThrowUnimplemented("FreeLlm");
}

float LLM::LLMImpl::GetEncodeTimings()
{
    // Placeholder: timing collection will be added with the real text path.
    ThrowUnimplemented("GetEncodeTimings");
}

float LLM::LLMImpl::GetDecodeTimings()
{
    // Placeholder: timing collection will be added with the real text path.
    ThrowUnimplemented("GetDecodeTimings");
}

void LLM::LLMImpl::ResetTimings()
{
    // Placeholder: timing reset will be added with the real text path.
    ThrowUnimplemented("ResetTimings");
}

std::string LLM::LLMImpl::SystemInfo()
{
    // Placeholder: replace with ExecuTorch runtime/system reporting.
    ThrowUnimplemented("SystemInfo");
}

void LLM::LLMImpl::ResetContext()
{
    // Placeholder: context reset will be added with the real text path.
    ThrowUnimplemented("ResetContext");
}

void LLM::LLMImpl::Encode(LlmChat::Payload& payload)
{
    (void)payload;
    // Placeholder: text encode flow is not implemented in the scaffold.
    ThrowUnimplemented("Encode");
}

std::string LLM::LLMImpl::NextToken()
{
    // Placeholder: token generation will be implemented with the runtime backend.
    ThrowUnimplemented("NextToken");
}

void LLM::LLMImpl::Cancel()
{
    // Placeholder: cancellation support will be added with the real text path.
    ThrowUnimplemented("Cancel");
}

size_t LLM::LLMImpl::GetChatProgress() const
{
    // Placeholder: progress tracking will be added with the real text path.
    ThrowUnimplemented("GetChatProgress");
}

void LLM::LLMImpl::StopGeneration()
{
    // Placeholder: generation stop support will be added with the real text path.
    ThrowUnimplemented("StopGeneration");
}

bool LLM::LLMImpl::ApplyAutoChatTemplate(LlmChat::Payload& payload)
{
    (void)payload;
    // Placeholder: ExecuTorch-specific auto chat templating is not implemented yet.
    ThrowUnimplemented("ApplyAutoChatTemplate");
}

std::string LLM::LLMImpl::GeneratePromptWithNumTokens(size_t numPromptTokens)
{
    (void)numPromptTokens;
    // Placeholder: benchmark prompt shaping is synthetic until tokenization exists.
    ThrowUnimplemented("GeneratePromptWithNumTokens");
}

void LLM::LLMImpl::ThrowUnimplemented(const char* operation) const
{
    if (!m_initialized) {
        THROW_ERROR("ExecuTorch %s failed: backend not initialized", operation);
    }

    THROW_ERROR("ExecuTorch %s failed: backend skeleton only; text-path implementation pending", operation);
}
