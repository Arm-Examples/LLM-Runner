//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmConfig.hpp"
#include <stdexcept>

LlmConfig::LlmConfig(const std::string& modelTag,
                     const std::string& modelPath,
                     const std::string& llmPrefix,
                     int numThreads,
                     int batchSize) :
    m_modelTag(modelTag), m_modelPath(modelPath), m_llmPrefix(llmPrefix)
{
    SetNumThreads(numThreads);
    SetBatchSize(batchSize);
}

std::string LlmConfig::GetModelTag() const
{
    return this->m_modelTag;
}

std::string LlmConfig::GetModelPath() const
{
    return this->m_modelPath;
}

std::string LlmConfig::GetLlmPrefix() const
{
    return this->m_llmPrefix;
}

int LlmConfig::GetNumThreads() const
{
    return this->m_numThreads;
}

int LlmConfig::GetBatchSize() const
{
    return this->m_batchSize;
}

void LlmConfig::SetModelTag(const std::string& modelIdentifier)
{
    this->m_modelTag = modelIdentifier;
}

void LlmConfig::SetModelPath(const std::string& basePath)
{
    this->m_modelPath = basePath;
}

void LlmConfig::SetLlmPrefix(const std::string& llmInitialPrompt)
{
    this->m_llmPrefix = llmInitialPrompt;
}

void LlmConfig::SetNumThreads(int threads)
{
    if (threads <= 0) {
        throw std::invalid_argument("number of threads must be a positive integer.");
    }
    this->m_numThreads = threads;
}

void LlmConfig::SetBatchSize(int batchSz)
{
    if (batchSz <= 0) {
        throw std::invalid_argument("batch-size must be a positive integer.");
    }
    this->m_batchSize = batchSz;
}
