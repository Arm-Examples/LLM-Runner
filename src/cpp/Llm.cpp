//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmImpl.hpp"
#include "LlmFactory.hpp"
#include <stdexcept>
#include <algorithm>

LLM::LLM(const LlmConfig &llmConfig) {
    LLMFactory factory;
    this->m_impl = factory.CreateLLMImpl(llmConfig);
    this->m_config = llmConfig;
}

LLM::~LLM() {
    this->FreeLlm();
}

void LLM::LlmInit(const LlmConfig &llmConfig)
{
    this->m_config = llmConfig;
    this->m_maxStopWordLength = 0;
    const auto &stopWords = this->m_config.GetStopWords();

    // Find the size of token buffer to hold tokens before emitting.
    if (!stopWords.empty()) {
        for (const auto &word: stopWords) {
            if (this->m_maxStopWordLength < word.size()) {
                this->m_maxStopWordLength = word.size();
            }
        }
    }

    if (this->m_maxStopWordLength < 1) {
        this->m_maxStopWordLength = 1;
    }
    this->m_impl->LlmInit(llmConfig);
}

void LLM::FreeLlm()
{
    this->m_impl->FreeLlm();
    this->m_maxStopWordLength = 1;
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

void LLM::Encode(EncodePayload& payload) {
    if (!m_impl) {
        throw std::runtime_error("LLM not initialized");
    }
    const std::vector<std::string> &inptMods = m_impl->SupportedInputModalities();

    if(payload.textPrompt != "") {
        bool supportsText = SupportsModality(inptMods, "text");
        if(!supportsText) {
            throw std::runtime_error("Error. Attempting to Encode an unsupported Text payload");
        }
    }
    if(payload.imagePath != "") {
        bool supportsVision = SupportsModality(inptMods, "image");
        if(!supportsVision) {
            throw std::runtime_error("Error. Attempting to Encode an unsupported Image payload");
        }
    }
    std::string query = this->m_impl->QueryBuilder(payload);
    payload.textPrompt = query;
    m_impl->Encode(payload);
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
    this->m_tokenBuffer += token;

    return this->ContainsStopWord();
}

size_t LLM::GetChatProgress() const
{
    return this->m_impl->GetChatProgress();
}

std::string LLM::BenchModel(int &nPrompts, int &nEvalPrompts, int &nMaxSeq, int &nRep)
{
    return this->m_impl->BenchModel(nPrompts, nEvalPrompts, nMaxSeq, nRep);
}

std::string LLM::GetFrameworkType() const
{
    return this->m_impl->GetFrameworkType();
}

std::vector<std::string> LLM::SupportedInputModalities() const
{
    return this->m_impl->SupportedInputModalities();
}

std::string LLM::ContainsStopWord()
{
    if (this->m_stopFlag) {
        this->m_stopFlag = false;
        this->m_tokenBuffer.clear();
        StopGeneration();
        return endToken;
    }
    //Detect Stop Word
    for (auto &w: this->m_config.GetStopWords()) {
        if (auto nPos = m_tokenBuffer.find(w); nPos != std::string::npos) {
            this->m_stopFlag = true;
            return m_tokenBuffer.substr(0, nPos);
        }
    }

    // Emit the tokens which is certainly not initial part of stop-words.
    if (m_tokenBuffer.length() >= this->m_maxStopWordLength)
    {
        std::string result = m_tokenBuffer.substr(0, m_tokenBuffer.length()
                                                     - m_maxStopWordLength);
        m_tokenBuffer.erase(0, m_tokenBuffer.length()
                               - m_maxStopWordLength);
        return result;
    }
    return "";
}

void LLM::StopGeneration()
{
    this->m_impl->StopGeneration();
}
