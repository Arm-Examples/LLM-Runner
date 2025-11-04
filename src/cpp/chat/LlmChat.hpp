//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "LlmConfig.hpp"


/**
 * @class LlmChat
 * @brief Basic chat helper for LLM frameworks.
 * Handles chat prompt construction using templates and
 * allows derived classes to apply custom formatting.
 */
class LlmChat {
public:
    /** Default constructor.*/
    LlmChat() = default;

    /** Virtual destructor.*/
    virtual ~LlmChat() = default;

    /**
     * Construct with chat parameters.
     * @param chatParams Chat parameters config with template fields.
     */
    explicit LlmChat(const ChatParams& chatParams)
        : m_systemTemplate(chatParams.systemTemplate),
          m_userTemplate(chatParams.userTemplate),
          m_systemPrompt(chatParams.systemPrompt),
          m_isDefaultChatTemplate(chatParams.applyDefaultChatTemplate) {}


    /** Prints the current template settings (for debugging).*/
    void Print() const;

    /**
     * @struct Payload
     * @brief Input payload for encoding a prompt.
     *
     * Encapsulates the parameters required when sending a prompt
     * (text, optional image, and conversation metadata) to the model.
     */
    struct Payload {
        std::string textPrompt;     ///< Text query to encode
        std::string imagePath;      ///< Path to image (optional, leave empty if none)
        bool isFirstMessage{false}; ///< Whether this is the first conversation message
    };

    /**
     * Applies the default %s-based chat template.
     * Replaces %s in user/system templates with payload text and system prompt.
     * @param payload prompt object to modify in place
     * @throws std::runtime_error if %s is missing in required templates
     */
    void ApplyDefaultChatTemplate(Payload& payload);

    /**
     * Initializes chat parameters from a JSON object.
     * @param chatParams Chat parameters config with template fields.
     */
    void InitChatParams(const ChatParams& chatParams) {
        m_systemTemplate = chatParams.systemTemplate;
        m_userTemplate   = chatParams.userTemplate;
        m_systemPrompt   = chatParams.systemPrompt;
        m_isDefaultChatTemplate = chatParams.applyDefaultChatTemplate;
    }

    /**
     * Builds a chat query using framework or default template.
     * @param payload The payload to process
     */
    virtual void QueryBuilder(Payload& payload) {
        if (!m_isDefaultChatTemplate) {
            if (ApplyAutoChatTemplate(payload)) {
                m_isConversationStart = false;
                return;  // Applied successfully
            }
        }
        // Fallback to default template
        ApplyDefaultChatTemplate(payload);
        m_isConversationStart = false;
    }

    /** System message template used for formatting. */
    std::string m_systemTemplate;
    /** User message template used for formatting. */
    std::string m_userTemplate;
    /** System-level prompt text prepended to the conversation. */
    std::string m_systemPrompt;
    /** Whether to apply the default chat template or use framework-specific logic. */
    bool m_isDefaultChatTemplate;
    /** Placeholder token used for substituting prompt content. */
    const char* m_templatePlaceholder = "%s";
    /** Tracks whether the current turn starts a new conversation. */
    bool m_isConversationStart = true;

protected:
    /**
     * Framework-specific chat template hook.
     * Override in derived classes if needed.
     * @param payload Input/output payload
     * @return true if handled, false otherwise
     */
    virtual bool ApplyAutoChatTemplate(Payload& payload) { return false; }
};
