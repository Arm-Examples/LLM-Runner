//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmImpl.hpp"
#include "LlmFactory.hpp"
#include <stdexcept>
#include <algorithm>
#include "Logger.hpp"
#include "LlmBridge.hpp"

LLM::LLM() {}

LLM::~LLM()
{
    this->FreeLlm();
}

void LLM::LlmInit(const LlmConfig &llmConfig, std::string sharedLibraryPath)
{
    LLMFactory factory;
    this->m_impl = factory.CreateLLMImpl(llmConfig);
    this->m_config = llmConfig;
    this->m_impl->InitChatParams(this->m_config.GetChat());
    const auto &stopWords = this->m_config.GetStopWords();

    this->m_impl->LlmInit(this->m_config, sharedLibraryPath);
}

void LLM::FreeLlm()
{
    this->m_impl->FreeLlm();
}

float LLM::GetEncodeTimings() const
{
    return this->m_impl->GetEncodeTimings();
}

float LLM::GetDecodeTimings() const
{
    return this->m_impl->GetDecodeTimings();
}

void LLM::ResetTimings()
{
    this->m_impl->ResetTimings();
}

std::string LLM::SystemInfo() const
{
    return this->m_impl->SystemInfo();
}

void LLM::ResetContext()
{
    this->m_impl->ResetContext();
}

void LLM::Encode(LlmChat::Payload& payload) {
    if (!m_impl) {
        THROW_ERROR("LLM not initialized");
    }
    const std::vector<std::string> &inptMods = m_impl->SupportedInputModalities();

    if(payload.textPrompt != "") {
        bool supportsText = SupportsModality(inptMods, "text");
        if(!supportsText) {
            THROW_ERROR("Error. Attempting to Encode an unsupported Text payload");
        }
    }
    if(payload.imagePath != "") {
        bool supportsVision = SupportsModality(inptMods, "image");
        if(!supportsVision) {
            THROW_ERROR("Error. Attempting to Encode an unsupported Image payload");
        }
    }
    this->m_impl->QueryBuilder(payload);
    this->m_impl->Encode(payload);
}

bool LLM::SupportsModality(const std::vector<std::string> &inptMods, std::string modality) const {
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return s;
    };

    bool supportsText = std::any_of(inptMods.begin(),
                                    inptMods.end(),
                                    [&](const std::string& s) {
                                        return toLower(s) == modality;
                                    });
    return supportsText;
}

std::string LLM::NextToken()
{
    auto token = this->m_impl->NextToken();

    if (this->isStopToken(token)) {
        return endToken;
    } else {
        return token;
    }
}

std::string LLM::CancellableNextToken(long operationId) const
{
    auto state = std::make_shared<WorkState>();
    state->operationId = operationId;
    addWork(state);

    std::string nextToken = this->m_impl->NextToken();

    auto work = removeWork(state->operationId);

    // Check for cancel
    if (work->cancelled.load(std::memory_order_acquire)) {

        // We support the option to only build the C++ bindings. This complier directive allows
        // only the C++ binding to be build and prevents the library attempting to trigger the
        // callback to the Java layer.
#if defined(JNI_FOUND)
        deliverCompletion(state->operationId, RESULT_CANCELLED, "cancelled");
#endif

        // Returned value won't be used, because operation will be cancelled.
        // But send end token just in case
        return  endToken;
    }
    for (auto &stopWord: this->m_config.GetStopWords()) {
        if (nextToken == stopWord) {
            return endToken;
        }
    }
    return nextToken;
}

void LLM::Cancel(long operationId)
{
    auto state = findWork(operationId);
    if (!state) {
        return;
    }
    state->cancelled.store(true, std::memory_order_release);
    
    
    this->m_impl->Cancel();
}

size_t LLM::GetChatProgress() const
{
    return this->m_impl->GetChatProgress();
}

std::string LLM::GetFrameworkType()
{
    return LLM::LLMImpl::GetFrameworkType();
}

std::vector<std::string> LLM::SupportedInputModalities() const
{
    return this->m_impl->SupportedInputModalities();
}

bool LLM::isStopToken(std::string token)
{
   for (auto &stopToken: this->m_config.GetStopWords()) {
        if (token == stopToken) {
            return true;
        }
    }

    return false;
}

std::string LLM::GeneratePromptWithNumTokens(size_t numPromptTokens)
{
    return this->m_impl->GeneratePromptWithNumTokens(numPromptTokens);
}

void LLM::StopGeneration()
{
    this->m_impl->StopGeneration();
}
