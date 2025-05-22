//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmImpl.hpp"

LLM::LLM()
{
    this->m_impl = std::make_unique<LLM::LLMImpl>();
}

LLM::~LLM()
{
    this->FreeLlm();
}

void LLM::LlmInit(const LlmConfig& llmConfig)
{
    this->m_impl->LlmInit(llmConfig);
}

void LLM::FreeLlm()
{
    this->m_impl->FreeLlm();
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
    this->m_impl->Encode(text);
}

std::string LLM::NextToken()
{
    return this->m_impl->NextToken();
}

size_t LLM::GetChatProgress()
{
    return this->m_impl->GetChatProgress();
}

std::string LLM::BenchModel(int& nPrompts, int& nEvalPrompts, int& nMaxSeq, int& nRep)
{
    return this->m_impl->BenchModel(nPrompts, nEvalPrompts, nMaxSeq, nRep);
}
