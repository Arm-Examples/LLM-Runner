//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmImpl.hpp"
#include "Logger.hpp"

LLM::LLMImpl::LLMImpl() {}

LLM::LLMImpl::~LLMImpl() {
    this->FreeLlm();
}

void LLM::LLMImpl::LlmInit(const LlmConfig& config, std::string sharedLibraryPath = "") {
    this->m_config            = config;
    this->m_numOfThreads      = config.GetConfigInt(LlmConfig::ConfigParam::NumThreads);
    this->m_nCtx              = config.GetConfigInt(LlmConfig::ConfigParam::ContextSize);
    this->m_modelPath         = config.GetConfigString(LlmConfig::ConfigParam::LlmModelName).c_str();

    if(this->m_modelPath.empty()) {
        THROW_ERROR("LLM initialization failed: model path is empty.");
    }
    this->m_llm = std::unique_ptr<MNN::Transformer::Llm>(
        MNN::Transformer::Llm::createLLM(this->m_modelPath.c_str()));

    if (!this->m_llm) {
        THROW_ERROR("LLM initialization failed: LLM instance not created.");
    }

    SetConfig();

    if (!m_llm->load()) {
        THROW_ERROR("LLM initialization failed: LLM model not loaded");
    }

    this->m_ctx = m_llm->getContext();
    if (!m_ctx) {
        THROW_ERROR("LLM initialization failed: LLM context not found.");
    }
}

void LLM::LLMImpl::SetConfig() {
     if (!this->m_llm) {
        THROW_ERROR("SetConfig called before LLM instance was created.");
    }

    std::ostringstream cfg;
    cfg << "{"
        << "\"thread_num\" : " << m_numOfThreads << ","
        << "\"reuse_kv\": true,"
        << "\"sampler_type\": \"greedy\","
        << "\"top_k\": 1,"
        << "\"top_p\": 1.0,"
        << "\"temperature\": 0.0,"
        << "\"max_all_tokens\": " << m_nCtx
        << "}";

    m_llm->set_config(cfg.str());
}

void LLM::LLMImpl::FreeLlm() {
    if(this->m_llm) {
        this->m_llm->reset();
        this->m_llm.reset();
    }
}

float LLM::LLMImpl::GetEncodeTimings(){
    return (m_ctx->prefill_us > 0) ? (m_ctx->prompt_len * 1e6f / m_ctx->prefill_us) : 0.0f;
}

float LLM::LLMImpl::GetDecodeTimings(){
    return (m_ctx->decode_us > 0) ? (m_ctx->gen_seq_len * 1e6f / m_ctx->decode_us) : 0.0f;
}

void LLM::LLMImpl::ResetTimings() {}

std::string LLM::LLMImpl::SystemInfo() {return "";}

void LLM::LLMImpl::ResetContext() {
    m_llm->reset();
    this->m_isConversationStart = true;
}

void LLM::LLMImpl::Encode(LlmChat::Payload& payload) {
    // Initialize generation context
    m_llm->generate_init(nullptr, nullptr);
    // Tokenize the input prompt into model-specific token IDs
    auto token_ids = m_llm->tokenizer_encode(payload.textPrompt);
    if (this->m_ctx->all_seq_len + token_ids.size() >= this->m_nCtx) {
        THROW_ERROR("LLM encoding failed ,context is full");
    }
    // Run the model once to encode the context; max_tokens=0 to skip decoding
    this->m_llm->generate(/*input_ids=*/token_ids, /*max_tokens=*/0);
}

std::string LLM::LLMImpl::NextToken() {
    // Generate the next token
    m_llm->generate(/*max_token=*/1);
    // Retrieve the most recent token ID from the output buffer
    int token_id = m_ctx->output_tokens.back();
    // Check if the model signaled a stop condition
    if (this->m_llm->is_stop(token_id)) {
        return m_eos;
    }
    // Decode the token ID back into a string and return it
    return m_llm->tokenizer_decode(token_id);
}

void LLM::LLMImpl::Cancel() {
    LOG_INF("Cancelling current operation");
}

size_t LLM::LLMImpl::GetChatProgress() {
    return 100 * m_ctx->all_seq_len / this->m_nCtx;
}

bool LLM::LLMImpl::ApplyAutoChatTemplate(LlmChat::Payload& payload)
{
    if(!this->m_llm) {
        THROW_ERROR("Failed to apply Chat Template, LLM not found.");
    }

    try {
        std::vector<MNN::Transformer::ChatMessage> message;
        // If this is the first message in a new conversation, prepend a system turn.
        if (this->m_isConversationStart) {
            message.push_back(std::make_pair("system", this->m_systemPrompt));
        }

        message.push_back(std::make_pair("user", payload.textPrompt));
        payload.textPrompt = this->m_llm->apply_chat_template(message);
        return true;
    } catch (const std::exception& e) {
        // Fallback to default implementation if auto failed or produced empty output
        LOG_WARN("ApplyChatTemplate failed. Falling back to default template");
        return false;
    }
}

std::vector<std::string> LLM::LLMImpl::SupportedInputModalities() const {
    std::vector<std::string> modalities = {"text"};
    if (m_llm) {
        auto config = nlohmann::json::parse(m_llm->dump_config());
        if (config.contains("is_visual") && config["is_visual"].get<bool>()) {
            modalities.push_back("image");
        }
    } else {
        THROW_ERROR("Failed to get input modalities: initialize LLM first.");
    }
    return modalities;
}

std::string LLM::LLMImpl::GeneratePromptWithNumTokens(size_t numPromptTokens)
{
    if (numPromptTokens == 0) {
        return std::string{};
    }

    // Simple base pattern. You can tweak this if needed.
    const std::string pattern = " A";

    std::string prompt;
    std::vector<int> token_ids;

    while (true) {
        prompt += pattern;

        // Use the real MNN tokenizer to see how many tokens we currently have
        token_ids = m_llm->tokenizer_encode(prompt);

        const size_t currentTokens = token_ids.size();

        if (currentTokens == numPromptTokens) {
            // Exact match – best case
            return prompt;
        }

        if (currentTokens > numPromptTokens) {
            // Overshoot: return best-effort
            return prompt;
        }
    }
}

void LLM::LLMImpl::StopGeneration() {}
