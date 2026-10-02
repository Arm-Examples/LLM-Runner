//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmImpl.hpp"
#include "Logger.hpp"

#include <cmath>
#include <cstdint>

namespace {

void PopulateMnnVisionStats(const MNN::Transformer::LlmContext& ctx, LLM::InferenceStats& stats)
{
    const double processedMegapixels = static_cast<double>(ctx.pixels_mp);
    if (processedMegapixels > 0.0) {
        stats.processedImagePixels = static_cast<std::uint64_t>(std::llround(processedMegapixels * 1000000.0));
        stats.processedImagePixelsSource = "mnn_context_pixels_mp";
    }

    const double visionUs = static_cast<double>(ctx.vision_us);
    if (visionUs > 0.0) {
        stats.visionTimeMs = visionUs / 1000.0;
    }
}

} // namespace

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
        << "\"jinja\": {\"context\": {\"enable_thinking\": false}},"
        << "\"max_new_tokens\": " << m_nCtx
        << "}";

    m_llm->set_config(cfg.str());
}

void LLM::LLMImpl::FreeLlm() {
    m_ctx = nullptr;
    m_lastInferenceStats = InferenceStats{};
    m_lastTerminationReason = TerminationReason::None;

    if(this->m_llm) {
        this->m_llm->reset();
        this->m_llm.reset();
    }
}

float LLM::LLMImpl::GetEncodeTimings(){
    if (!m_ctx || m_ctx->prefill_us <= 0) {
        return 0.0f;
    }
    return (m_ctx->prompt_len * 1e6f) / m_ctx->prefill_us;
}

float LLM::LLMImpl::GetDecodeTimings(){
    if (!m_ctx || m_ctx->decode_us <= 0) {
        return 0.0f;
    }
    return (m_ctx->gen_seq_len * 1e6f) / m_ctx->decode_us;
}

void LLM::LLMImpl::ResetTimings() {
    m_lastInferenceStats = InferenceStats{};
}

std::string LLM::LLMImpl::SystemInfo() {return "";}

void LLM::LLMImpl::ResetContext() {
    if (!m_llm) {
        THROW_ERROR("ResetContext called before model initialization.");
    }

    m_llm->reset();
    this->m_isConversationStart = true;
    m_lastTerminationReason = TerminationReason::None;
}

std::vector<int> LLM::LLMImpl::BuildAndTokenizeInput(const LlmChat::Payload& payload)
{
    // LLM::Encode invokes QueryBuilder before this method. For MNN vision,
    // QueryBuilder materializes the image path in the <img><hw>...</hw>...</img>
    // prompt markup that MNN uses to resolve multimodal inputs.
    return m_llm->tokenizer_encode(payload.textPrompt);
}

void LLM::LLMImpl::Encode(LlmChat::Payload& payload, InferenceStats* inferenceStats) {
    if (!m_llm || !m_ctx) {
        THROW_ERROR("Encode called before model initialization.");
    }

    m_lastInferenceStats = InferenceStats{};
    m_collectInferenceStats = inferenceStats != nullptr;
    m_lastTerminationReason = TerminationReason::None;

    // Initialize generation context
    m_llm->generate_init(nullptr, nullptr);

    const bool hasImagePayload = !payload.imagePath.empty();
    auto token_ids = BuildAndTokenizeInput(payload);
    if (m_collectInferenceStats) {
        m_lastInferenceStats.textPromptTokens = token_ids.size();
    }
    if (this->m_ctx->all_seq_len + token_ids.size() >= this->m_nCtx) {
        m_lastTerminationReason = TerminationReason::ContextFull;
        THROW_ERROR("LLM encoding failed, context is full");
    }
    // Run the model once to encode the context; max_tokens=0 to skip decoding
    this->m_llm->generate(/*input_ids=*/token_ids, /*max_tokens=*/0);

    // MNN prompt_len currently matches text prompt tokens for this vision path,
    // so do not report it as a fused text+vision position count.
    if (m_collectInferenceStats && m_ctx->prefill_us > 0) {
        m_lastInferenceStats.prefillTimeMs = static_cast<double>(m_ctx->prefill_us) / 1000.0;
    }
    if (m_collectInferenceStats && hasImagePayload) {
        PopulateMnnVisionStats(*m_ctx, m_lastInferenceStats);
    }
    if (inferenceStats) {
        *inferenceStats = m_lastInferenceStats;
    }
}

void LLM::LLMImpl::UpdateDecodeStats()
{
    if (m_collectInferenceStats && m_ctx->decode_us > 0) {
        m_lastInferenceStats.decodeTimeMs = static_cast<double>(m_ctx->decode_us) / 1000.0;
    }
}

std::optional<LLM::TextTokenId> LLM::LLMImpl::NextTokenId() {
    if (!m_llm || !m_ctx) {
        THROW_ERROR("NextTokenId called before model initialization.");
    }

    const std::size_t previousSize = m_ctx->output_tokens.size();
    m_llm->generate(/*max_token=*/1);
    UpdateDecodeStats();

    if (m_ctx->output_tokens.size() <= previousSize) {
        m_lastTerminationReason = TerminationReason::BackendEos;
        return std::nullopt;
    }

    const LLM::TextTokenId tokenId = static_cast<LLM::TextTokenId>(m_ctx->output_tokens.back());
    if (this->m_llm->is_stop(tokenId)) {
        m_lastTerminationReason = TerminationReason::BackendEos;
        return std::nullopt;
    }
    m_lastTerminationReason = TerminationReason::None;
    return tokenId;
}

std::string LLM::LLMImpl::DetokenizeTextToken(TextTokenId token)
{
    if (!m_llm) {
        THROW_ERROR("DetokenizeTextToken called before model initialization.");
    }
    return m_llm->tokenizer_decode(token);
}

void LLM::LLMImpl::Cancel() {
    LOG_INF("Cancelling current operation");
}

size_t LLM::LLMImpl::GetChatProgress() {
    if (!m_ctx || m_nCtx <= 0) {
        THROW_ERROR("GetChatProgress called before model initialization.");
    }
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

std::string LLM::LLMImpl::GeneratePromptWithNumTokens(const size_t numPromptTokens)
{
    if (!m_llm) {
        THROW_ERROR("GeneratePromptWithNumTokens called before model initialization.");
    }
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
