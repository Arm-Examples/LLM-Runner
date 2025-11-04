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


static std::string vformat(const char* format, va_list args)
{
    va_list tmp;
    va_copy(tmp, args);
    int num_chars = std::vsnprintf(nullptr, 0, format, tmp);
    va_end(tmp);

    if (num_chars <= 0) return "";

    std::vector<char> buf(num_chars + 1);
    std::vsnprintf(buf.data(), buf.size(), format, args);
    return std::string(buf.data(), num_chars);
}

static std::string formatString(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    std::string result = vformat(format, args);
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
        userTurn = formatString(m_userTemplate.c_str(), payload.textPrompt.c_str());
    } else {
        LOG_INF("[Warning] userTemplate is missing '%s'; using raw text.");
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
        systemTurn = formatString(m_systemTemplate.c_str(), m_systemPrompt.c_str());
    } else {
        LOG_INF("[Warning] systemTemplate is missing '%s'; prepending raw system prompt.");
        systemTurn = m_systemPrompt;
    }

    payload.textPrompt.reserve(systemTurn.size() + userTurn.size());
    payload.textPrompt = std::move(systemTurn) + std::move(userTurn);
}
