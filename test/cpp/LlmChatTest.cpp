//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmChat.hpp"

#include "catch2/catch_test_macros.hpp"

namespace {

ChatParams MakeChatParams(
    std::string systemTemplate = "S:%s\n",
    std::string userTemplate = "U:%s\n",
    std::string systemPrompt = "system",
    bool applyDefaultChatTemplate = true)
{
    ChatParams params{};
    params.systemTemplate = std::move(systemTemplate);
    params.userTemplate = std::move(userTemplate);
    params.systemPrompt = std::move(systemPrompt);
    params.applyDefaultChatTemplate = applyDefaultChatTemplate;
    return params;
}

class TestAutoChat final : public LlmChat {
public:
    explicit TestAutoChat(bool shouldApply)
        : m_shouldApply(shouldApply)
    {
    }

protected:
    bool ApplyAutoChatTemplate(Payload& payload) override
    {
        if (!m_shouldApply) {
            return false;
        }

        payload.textPrompt = "AUTO:" + payload.textPrompt;
        return true;
    }

private:
    bool m_shouldApply;
};

} // namespace

TEST_CASE("LlmChat: first message applies system and user templates")
{
    LlmChat chat(MakeChatParams());

    LlmChat::Payload payload{"hello", "", true};

    chat.ApplyDefaultChatTemplate(payload);

    CHECK(payload.textPrompt == "S:system\nU:hello\n");
    CHECK(payload.imagePath.empty());
}

TEST_CASE("LlmChat: subsequent message only applies user template")
{
    LlmChat chat(MakeChatParams());
    chat.m_isConversationStart = false;

    LlmChat::Payload payload{"next", "", false};

    chat.ApplyDefaultChatTemplate(payload);

    CHECK(payload.textPrompt == "U:next\n");
    CHECK(payload.imagePath.empty());
}

TEST_CASE("LlmChat: missing user placeholder falls back to raw user prompt")
{
    LlmChat chat(MakeChatParams(
        "S:%s\n",
        "USER TEMPLATE WITHOUT PLACEHOLDER",
        "system",
        true));

    LlmChat::Payload payload{"hello", "", true};

    chat.ApplyDefaultChatTemplate(payload);

    CHECK(payload.textPrompt == "S:system\nhello");
}

TEST_CASE("LlmChat: missing system placeholder falls back to raw system prompt")
{
    LlmChat chat(MakeChatParams(
        "SYSTEM TEMPLATE WITHOUT PLACEHOLDER",
        "U:%s\n",
        "system",
        true));

    LlmChat::Payload payload{"hello", "", true};

    chat.ApplyDefaultChatTemplate(payload);

    CHECK(payload.textPrompt == "systemU:hello\n");
}

TEST_CASE("LlmChat: missing system and user placeholders falls back to raw prompts")
{
    LlmChat chat(MakeChatParams(
        "SYSTEM TEMPLATE WITHOUT PLACEHOLDER",
        "USER TEMPLATE WITHOUT PLACEHOLDER",
        "system",
        true));

    LlmChat::Payload payload{"hello", "", true};

    chat.ApplyDefaultChatTemplate(payload);

    CHECK(payload.textPrompt == "systemhello");
}

TEST_CASE("LlmChat: empty user prompt formats correctly")
{
    LlmChat chat(MakeChatParams());

    LlmChat::Payload payload{"", "", true};

    chat.ApplyDefaultChatTemplate(payload);

    CHECK(payload.textPrompt == "S:system\nU:\n");
}

TEST_CASE("LlmChat: empty system prompt formats correctly")
{
    LlmChat chat(MakeChatParams(
        "S:%s\n",
        "U:%s\n",
        "",
        true));

    LlmChat::Payload payload{"hello", "", true};

    chat.ApplyDefaultChatTemplate(payload);

    CHECK(payload.textPrompt == "S:\nU:hello\n");
}

TEST_CASE("LlmChat: image path is preserved when formatting prompt")
{
    LlmChat chat(MakeChatParams());

    LlmChat::Payload payload{"hello", "/tmp/image.bmp", true};

    chat.ApplyDefaultChatTemplate(payload);

    CHECK(payload.textPrompt == "S:system\nU:hello\n");
    CHECK(payload.imagePath == "/tmp/image.bmp");
}

TEST_CASE("LlmChat: QueryBuilder applies default template when configured")
{
    LlmChat chat(MakeChatParams(
        "S:%s\n",
        "U:%s\n",
        "system",
        true));

    LlmChat::Payload payload{"hello", "", true};

    chat.QueryBuilder(payload);

    CHECK(payload.textPrompt == "S:system\nU:hello\n");
    CHECK(chat.m_isConversationStart == false);
}

TEST_CASE("LlmChat: QueryBuilder applies auto template when default template is disabled and auto succeeds")
{
    TestAutoChat chat(true);
    chat.InitChatParams(MakeChatParams(
        "S:%s\n",
        "U:%s\n",
        "system",
        false));

    LlmChat::Payload payload{"hello", "", true};

    chat.QueryBuilder(payload);

    CHECK(payload.textPrompt == "AUTO:hello");
    CHECK(chat.m_isConversationStart == false);
}

TEST_CASE("LlmChat: QueryBuilder falls back to default template when auto template fails")
{
    TestAutoChat chat(false);
    chat.InitChatParams(MakeChatParams(
        "S:%s\n",
        "U:%s\n",
        "system",
        false));

    LlmChat::Payload payload{"hello", "", true};

    chat.QueryBuilder(payload);

    CHECK(payload.textPrompt == "S:system\nU:hello\n");
    CHECK(chat.m_isConversationStart == false);
}

TEST_CASE("LlmChat: InitChatParams updates existing chat parameters")
{
    LlmChat chat;

    chat.InitChatParams(MakeChatParams(
        "SYS:%s;",
        "USR:%s;",
        "new-system",
        true));

    LlmChat::Payload payload{"new-user", "", true};

    chat.QueryBuilder(payload);

    CHECK(payload.textPrompt == "SYS:new-system;USR:new-user;");
}
TEST_CASE("LlmChat: QueryBuilder transitions conversation state correctly")
{
    LlmChat chat(MakeChatParams());

    CHECK(chat.m_isConversationStart == true);

    LlmChat::Payload firstPayload{"first message", "", true};

    chat.QueryBuilder(firstPayload);

    CHECK(firstPayload.textPrompt == "S:system\nU:first message\n");
    CHECK(chat.m_isConversationStart == false);

    LlmChat::Payload secondPayload{"second message", "", false};

    chat.QueryBuilder(secondPayload);

    CHECK(secondPayload.textPrompt == "U:second message\n");
    CHECK(chat.m_isConversationStart == false);
}

TEST_CASE("LlmChat: special characters in prompts are preserved")
{
    LlmChat chat(MakeChatParams(
        "SYS:%s\n",
        "USR:%s\n",
        "system",
        true));

    LlmChat::Payload payload{"hello %x %d %s world", "", true};

    chat.ApplyDefaultChatTemplate(payload);

    CHECK(payload.textPrompt == "SYS:system\nUSR:hello %x %d %s world\n");
}