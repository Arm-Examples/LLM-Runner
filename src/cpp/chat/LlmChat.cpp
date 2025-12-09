//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmChat.hpp"
#include "Logger.hpp"
#include <iostream>
#include <string>
#include <vector>

void LlmChat::Print() const {
    std::cout << "System Template: " << m_systemTemplate << "\n"
              << "User Template: "   << m_userTemplate   << "\n"
              << "System Prompt: "   << m_systemPrompt   << "\n";
}


static std::string Vformat(const char* format, va_list args)
{
    if (!format) {
        LOG_ERROR("Vformat: null format string");
        return "";
    }
    va_list tmp;
    va_copy(tmp, args);
    int num_chars = std::vsnprintf(nullptr, 0, format, tmp);
    va_end(tmp);

    if (num_chars < 0) {
        LOG_ERROR("Vformat: vsnprintf failed during size calculation for format string \"%s\"", format);
        return "";
    }

    if (num_chars == 0) {
        LOG_DEBUG("Vformat: formatted output is empty for format string \"%s\"", format);
        return "";
    }

    std::vector<char> buf(static_cast<size_t>(num_chars) + 1);
    int written = std::vsnprintf(buf.data(), buf.size(), format, args);
    if (written < 0) {
        LOG_ERROR("Vformat: vsnprintf failed during formatting for format string \"%s\"", format);
    }

    return std::string(buf.data(), static_cast<size_t>(written));
}

static std::string FormatString(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::string result = Vformat(format, args);
    va_end(args);
    return result;
}

void LlmChat::ApplyDefaultChatTemplate(Payload& payload)
{
    const bool hasUserPlaceholder   = m_userTemplate.find(m_templatePlaceholder)   != std::string::npos;
    const bool hasSystemPlaceholder = m_systemTemplate.find(m_templatePlaceholder) != std::string::npos;

    // Build user turn (fallback: raw user prompt)
    std::string userTurn;
    if (hasUserPlaceholder) {
        userTurn = FormatString(m_userTemplate.c_str(), payload.textPrompt.c_str());
    } else {
        LOG_DEBUG("[Warning] userTemplate is missing \"%s\"; using raw text.", m_templatePlaceholder);
        userTurn = payload.textPrompt;
    }

    // If not conversation start, return user prompt
    if (!m_isConversationStart) {
        payload.textPrompt = std::move(userTurn);
        return;
    }

    // Build system turn (fallback: raw system prompt)
    std::string systemTurn;
    if (hasSystemPlaceholder) {
        systemTurn = FormatString(m_systemTemplate.c_str(), m_systemPrompt.c_str());
    } else {
        LOG_DEBUG("[Warning] systemTemplate is missing \"%s\"; prepending raw system prompt.", m_templatePlaceholder);
        systemTurn = m_systemPrompt;
    }

    payload.textPrompt.reserve(systemTurn.size() + userTurn.size());
    payload.textPrompt = std::move(systemTurn) + std::move(userTurn);
}
