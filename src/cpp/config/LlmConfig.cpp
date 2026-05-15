//
// SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmConfig.hpp"
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "Logger.hpp"

using nlohmann::json;

static inline const char* to_string(LlmConfig::ConfigParam key) {
  switch (key) {
    case LlmConfig::ConfigParam::SystemPrompt:            return "SystemPrompt";
    case LlmConfig::ConfigParam::SystemTemplate:          return "SystemTemplate";
    case LlmConfig::ConfigParam::UserTemplate:            return "UserTemplate";
    case LlmConfig::ConfigParam::LlmModelName:            return "LlmModelName";
    case LlmConfig::ConfigParam::ProjModelName:           return "ProjModelName";
    case LlmConfig::ConfigParam::ApplyDefaultChatTemplate:return "ApplyDefaultChatTemplate";
    case LlmConfig::ConfigParam::IsVision:                return "IsVision";
    case LlmConfig::ConfigParam::NumThreads:              return "NumThreads";
    case LlmConfig::ConfigParam::BatchSize:               return "BatchSize";
    case LlmConfig::ConfigParam::ContextSize:             return "ContextSize";
    case LlmConfig::ConfigParam::MaxInputDimension:       return "MaxInputDimension";
  }
  return "Unknown";
}

LlmConfig::LlmConfig(const std::string& jsonStr)
{
    json cfg;
    try {
        cfg = json::parse(jsonStr);
    } catch (const std::exception& e) {
        THROW_INVALID_ARGUMENT("config: schema/type error: %s", e.what());
    }

    // Required sections (throws via .at if missing)
    const json& chatJ    = cfg.at("chat");
    const json& modelJ   = cfg.at("model");
    const json& runtimeJ = cfg.at("runtime");


    // Parse directly into members (throws on missing/wrong types)
    try {
        chatJ.get_to(m_chat);
        runtimeJ.get_to(m_runtime);
        modelJ.get_to(m_model);
    } catch (const nlohmann::json::exception& e) {
        THROW_INVALID_ARGUMENT("config: schema/type error: %s", e.what());
    }

    // Basic invariants
    if (m_runtime.numThreads <= 0)
        THROW_INVALID_ARGUMENT("config.runtime.numThreads must be positive");
    if (m_runtime.batchSize <= 0)
        THROW_INVALID_ARGUMENT("config.runtime.batchSize must be positive");
    if (m_runtime.contextSize <= 0)
        THROW_INVALID_ARGUMENT("config.runtime.contextSize must be positive");
    if (m_model.maxInputDimension <= 0)
        THROW_INVALID_ARGUMENT("config.model.maxInputDimension must be positive");

    // stopWords: must exist, array, non-empty, all non-empty strings
    const json& sw = cfg.at("stopWords");
    if (!sw.is_array() || sw.empty())
        THROW_INVALID_ARGUMENT("config.stopWords must be a non-empty array of strings");

    std::vector<std::string> stopWords;
    stopWords.reserve(sw.size());
    for (const auto& v : sw) {
        if (!v.is_string())
            THROW_INVALID_ARGUMENT("config.stopWords: all entries must be strings");
        const std::string s = v.get<std::string>();
        if (s.empty())
            THROW_INVALID_ARGUMENT("config.stopWords: strings must be non-empty");
        stopWords.emplace_back(s);
    }
    m_stopWords = std::move(stopWords);
}

void LlmConfig::SetConfigString(ConfigParam key, const std::string& value) {
    switch (key) {
        case ConfigParam::SystemPrompt:     m_chat.systemPrompt   = value; return;
        case ConfigParam::SystemTemplate:   m_chat.systemTemplate = value; return;
        case ConfigParam::UserTemplate:     m_chat.userTemplate   = value; return;
        case ConfigParam::LlmModelName:     m_model.llmModelName  = value; return;
        case ConfigParam::ProjModelName:    m_model.projModelName = value; return;
        default: THROW_INVALID_ARGUMENT("Unknown string key: %s", to_string(key));
    }
}

void LlmConfig::SetConfigBool(ConfigParam key, bool value) {
    switch (key) {
        case ConfigParam::ApplyDefaultChatTemplate: m_chat.applyDefaultChatTemplate = value; return;
        case ConfigParam::IsVision:                 m_model.isVision                = value; return;
        THROW_INVALID_ARGUMENT("Unknown bool key: %s", to_string(key));
    }
}

void LlmConfig::SetConfigInt(ConfigParam key, int value) {
    switch (key) {
        case ConfigParam::NumThreads:
            if (value <= 0) {
                THROW_INVALID_ARGUMENT("NumThreads must be > 0");
            }
            m_runtime.numThreads = value; return;
        case ConfigParam::BatchSize:
            if (value <= 0) {
                THROW_INVALID_ARGUMENT("BatchSize must be > 0");
            }
            m_runtime.batchSize = value; return;
        case ConfigParam::ContextSize:
            if (value <= 0) {
                THROW_INVALID_ARGUMENT("ContextSize must be > 0");
            }
            m_runtime.contextSize = value; return;
        case ConfigParam::MaxInputDimension:
            if (value <= 0) {
                THROW_INVALID_ARGUMENT("MaxInputDimension must be > 0");
            }
            m_model.maxInputDimension = value; return;
        default: THROW_INVALID_ARGUMENT("Unknown int key: %s", to_string(key));
    }
}

[[nodiscard]] std::string LlmConfig::GetConfigString(ConfigParam key) const {
    switch (key) {
        case ConfigParam::SystemPrompt:     return m_chat.systemPrompt;
        case ConfigParam::SystemTemplate:   return m_chat.systemTemplate;
        case ConfigParam::UserTemplate:     return m_chat.userTemplate;
        case ConfigParam::LlmModelName:     return m_model.llmModelName;
        case ConfigParam::ProjModelName:    return m_model.projModelName;
        default: THROW_INVALID_ARGUMENT("Unknown string key: %s", to_string(key));
    }
}

[[nodiscard]] bool LlmConfig::GetConfigBool(ConfigParam key) const {
    switch (key) {
        case ConfigParam::ApplyDefaultChatTemplate: return m_chat.applyDefaultChatTemplate;
        case ConfigParam::IsVision:                 return m_model.isVision;
        default: THROW_INVALID_ARGUMENT("Unknown bool key: %s", to_string(key));
    }
}

[[nodiscard]] int LlmConfig::GetConfigInt(ConfigParam key) const {
    switch (key) {
        case ConfigParam::NumThreads:  return m_runtime.numThreads;
        case ConfigParam::BatchSize:   return m_runtime.batchSize;
        case ConfigParam::ContextSize: return m_runtime.contextSize;
        case ConfigParam::MaxInputDimension: return m_model.maxInputDimension;
        default: THROW_INVALID_ARGUMENT("Unknown int key: %s", to_string(key));
    }
}

void LlmConfig::SetStopWords(const std::vector<std::string>& stopWords)
{
    if (stopWords.empty()) {
        THROW_INVALID_ARGUMENT("config.stopWords: strings must be non-empty");
    }
    for (const auto& s : stopWords) {
        if (s.empty()) {
            THROW_INVALID_ARGUMENT("config.stopWords: all entries must be strings");
        }
    }
    this->m_stopWords = stopWords;
}
