//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmImpl.hpp"

LLM::LLM() {
    this->m_impl = std::make_unique<LLM::LLMImpl>();
}

LLM::~LLM() {
    this->FreeLlm();
}

void LLM::LlmInit(const LlmConfig &llmConfig)
{
    this->m_config = llmConfig;
    this->m_maxStopWordLength = 0;
    this->m_evaluatedOnce.store(false);
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
    this->m_evaluatedOnce.store(false);
    this->m_maxStopWordLength = 1;
}

float LLM::GetEncodeTimings()
{
    return this->m_impl->GetEncodeTimings();
}

float LLM::GetDecodeTimings()
{
    return this->m_impl->GetDecodeTimings();
}

void LLM::ResetTimings()
{
    this->m_impl->ResetTimings();
}

std::string LLM::SystemInfo()
{
    return this->m_impl->SystemInfo();
}

void LLM::ResetContext()
{
    this->m_impl->ResetContext();
}

void LLM::Encode(std::string text)
{
    std::string query = QueryBuilder(text);
    this->m_impl->Encode(query);
    this->m_evaluatedOnce.store(true);
}

std::string LLM::NextToken()
{
    auto token = this->m_impl->NextToken();
    this->m_tokenBuffer += token;

    return this->ContainsStopWord();
}

size_t LLM::GetChatProgress()
{
    return this->m_impl->GetChatProgress();
}

std::string LLM::BenchModel(int &nPrompts, int &nEvalPrompts, int &nMaxSeq, int &nRep)
{
    return this->m_impl->BenchModel(nPrompts, nEvalPrompts, nMaxSeq, nRep);
}

std::string LLM::GetFrameworkType()
{
    return this->m_impl->GetFrameworkType();
}

std::string LLM::QueryBuilder(std::string &prompt)
{

    const std::string prefix = this->m_evaluatedOnce.load() ? "" :this->m_config.GetLlmPrefix();

    return prefix + this->m_config.GetUserTag() + prompt + this->m_config.GetEndTag() + this->m_config.GetModelTag();
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
