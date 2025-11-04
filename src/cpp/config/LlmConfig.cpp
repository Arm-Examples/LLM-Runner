//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmConfig.hpp"
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "Logger.hpp"

using nlohmann::json;

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

void LlmConfig::SetConfigString(const std::string& key, const std::string& value) {
    if (key == "systemPrompt")   { m_chat.systemPrompt = value; return; }
    if (key == "systemTemplate") { m_chat.systemTemplate = value; return; }
    if (key == "userTemplate")   { m_chat.userTemplate = value; return; }
    if (key == "llmModelName")   { m_model.llmModelName = value; return; }
    if (key == "projModelName")  { m_model.projModelName = value; return; }
    THROW_INVALID_ARGUMENT("Unknown string key: %s", key.c_str());
}

void LlmConfig::SetConfigBool(const std::string& key, bool value) {
    if (key == "applyDefaultChatTemplate") { m_chat.applyDefaultChatTemplate = value; return; }
    if (key == "isVision")                 { m_model.isVision = value; return; }
    THROW_INVALID_ARGUMENT("Unknown bool key: %s", key.c_str());
}

void LlmConfig::SetConfigInt(const std::string& key, int value) {
    if (key == "numThreads") { m_runtime.numThreads = value; return; }
    if (key == "batchSize")  { m_runtime.batchSize  = value; return; }
    if (key == "contextSize"){ m_runtime.contextSize= value; return; }
    THROW_INVALID_ARGUMENT("Unknown int key: %s", key.c_str());
}

std::string LlmConfig::GetConfigString(const std::string& key) const {
    if (key == "systemPrompt")   return m_chat.systemPrompt;
    if (key == "systemTemplate") return m_chat.systemTemplate;
    if (key == "userTemplate")   return m_chat.userTemplate;
    if (key == "llmModelName")   return m_model.llmModelName;
    if (key == "projModelName")  return m_model.projModelName;
    THROW_INVALID_ARGUMENT("Unknown string key: %s", key.c_str());
}

bool LlmConfig::GetConfigBool(const std::string& key) const {
    if (key == "applyDefaultChatTemplate") return m_chat.applyDefaultChatTemplate;
    if (key == "isVision")                 return m_model.isVision;
    THROW_INVALID_ARGUMENT("Unknown bool key: %s", key.c_str());
}

int LlmConfig::GetConfigInt(const std::string& key) const {
    if (key == "numThreads") return m_runtime.numThreads;
    if (key == "batchSize")  return m_runtime.batchSize;
    if (key == "contextSize")return m_runtime.contextSize;
    THROW_INVALID_ARGUMENT("Unknown int key: %s", key.c_str());
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
