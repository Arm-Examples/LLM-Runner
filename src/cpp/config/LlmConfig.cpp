//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmConfig.hpp"
#include <stdexcept>
#include <nlohmann/json.hpp>

LlmConfig::LlmConfig(const std::string& jsonStr)
{
    nlohmann::json modelConfig;
    try {
        modelConfig = nlohmann::json::parse(jsonStr);
    } catch (const std::exception& e) {
        throw std::invalid_argument(std::string("Invalid JSON input: ") + e.what());
    }

    m_modelTag  = modelConfig.value("modelTag", "");
    m_userTag   = modelConfig.value("userTag", "");
    m_endTag    = modelConfig.value("endTag", "");
    m_mediaTag   = modelConfig.value("mediaTag", "");

    if (!modelConfig.contains("llmModelName")) {
        throw std::runtime_error("Missing required parameter: modelPath");
    }
    m_modelPath = modelConfig["llmModelName"];
    m_llmPrefix = modelConfig.value("llmPrefix", "");
    m_framework = modelConfig.value("framework", "");

    // Stop-words should be a non-empty array of string with no null strings.

    if (!modelConfig.contains("stopWords") || !modelConfig["stopWords"].is_array() || modelConfig["stopWords"].empty() )
    {
        throw std::invalid_argument("Missing 'stopWords' key or invalid 'stopWords', stopWords must be a non-empty array.");
    }

    std::vector<std::string> parsedStopWords;

    for (const auto& val : modelConfig["stopWords"]) {
        if (!val.is_string() || val.get<std::string>().empty()) {
            throw std::invalid_argument("All stopWords must be non-empty strings.");
        }
        parsedStopWords.emplace_back(val.get<std::string>());
    }

    std::vector<std::string> parsedInputModalities;
    for (const auto& val : modelConfig["inputModalities"]) {
        if (!val.is_string() || val.get<std::string>().empty()) {
            throw std::invalid_argument("All input modalities must be non-empty strings.");
        }
        parsedInputModalities.emplace_back(val.get<std::string>());
    }


    std::vector<std::string> parsedOutputModalities;
    for (const auto& val : modelConfig["outputModalities"]) {
        if (!val.is_string() || val.get<std::string>().empty()) {
            throw std::invalid_argument("All input modalities must be non-empty strings.");
        }
        parsedOutputModalities.emplace_back(val.get<std::string>());
    }

    SetStopWords(parsedStopWords);
    SetInputModalities(parsedInputModalities);
    SetOutputModalities(parsedOutputModalities);

    if(Contains(parsedInputModalities, "image")) {
        if(!modelConfig.contains("llmMmProjModelName")) {
            throw std::runtime_error("Missing required parameter: llmMmProjModelName");
        }
        m_mmProjModelPath = modelConfig["llmMmProjModelName"];
    }

    //Default Batchsize is set to 256 if not provided and number of threads to 4.
    SetNumThreads(modelConfig.value("numThreads", 4));
    SetBatchSize(modelConfig.value("batchSize", 256));
}

std::string LlmConfig::GetEndTag() const
{
    return this->m_endTag;
}

std::string LlmConfig::GetUserTag() const
{
    return this->m_userTag;
}

std::string LlmConfig::GetModelTag() const
{
    return this->m_modelTag;
}

std::string LlmConfig::GetMediaTag() const
{
    return this->m_mediaTag;
}

std::string LlmConfig::GetModelPath() const
{
    return this->m_modelPath;
}

std::string LlmConfig::GetMMPROJModelPath() const
{
    return this->m_mmProjModelPath;
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

std::vector<std::string> LlmConfig::GetStopWords() const
{
    return this->m_stopWords;
}

std::vector<std::string> LlmConfig::GetInputModalities() const
{
    return this->m_inputModalities;
}

std::vector<std::string> LlmConfig::GetOutputModalities() const
{
    return this->m_outputModalities;
}

void LlmConfig::SetModelTag(const std::string& modelIdentifier)
{
    this->m_modelTag = modelIdentifier;
}

void LlmConfig::SetUserTag(const std::string& userTag)
{
    this->m_userTag = userTag;
}

void LlmConfig::SetEndTag(const std::string& endTag)
{
    this->m_endTag = endTag;
}

void LlmConfig::SetModelPath(const std::string& basePath)
{
    this->m_modelPath = basePath;
}

void LlmConfig::SetMMPROJModelPath(const std::string& projectionModel)
{
    this->m_mmProjModelPath = projectionModel;
}

void LlmConfig::SetFramework(const std::string& framework)
{
    this->m_framework = framework;
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

void LlmConfig::SetStopWords(const std::vector<std::string>& stopWords)
{
    if (stopWords.empty()) {
        throw std::invalid_argument("Stop words must not be empty.");
    }
    this->m_stopWords = stopWords;
}

void LlmConfig::SetInputModalities(const std::vector<std::string>& inputModalities)
{
    if (inputModalities.empty()) {
        throw std::invalid_argument("Input Modalities must not be empty.");
    }
    this->m_inputModalities = inputModalities;
}

void LlmConfig::SetOutputModalities(const std::vector<std::string>& outputModalities)
{
    if (outputModalities.empty()) {
        throw std::invalid_argument("Output Modalities must not be empty.");
    }
    this->m_outputModalities = outputModalities;
}

void LlmConfig::ClearStopWords()
{
    this->m_stopWords.clear();
}

void LlmConfig::AddStopWord(const std::string& stopWord)
{
    this->m_stopWords.push_back(stopWord);
}

template <typename Container, typename T>
bool LlmConfig::Contains(const Container& container, const T& value) {
    return std::find(container.begin(), container.end(), value) != container.end();
}

void LlmConfig::AddInputModality(const std::string& inputModality) {
    this->m_inputModalities.emplace_back(inputModality);
}

void LlmConfig::AddOutputModality(const std::string& outputModality) {
    this->m_outputModalities.emplace_back(outputModality);
}
