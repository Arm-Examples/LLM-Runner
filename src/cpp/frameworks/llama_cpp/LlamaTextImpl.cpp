//
// SPDX-FileCopyrightText: Copyright 2024-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlamaTextImpl.hpp"
#include "is_utf8.h"
#include "Logger.hpp"

/**
 * @brief LLama Implementation of our LLM API
 *
 */
LLM::LLMImpl::LLMImpl() = default;

LLM::LLMImpl::~LLMImpl()
{
    this->FreeLlm();
}

void LLM::LLMImpl::LoadModel()
{
    const llama_model_params model_params = llama_model_default_params();
    const std::string& modelPath = this->m_config.GetConfigString(LlmConfig::ConfigParam::LlmModelName);
    if (modelPath.empty()) {
        THROW_ERROR("Model path supplied in config is empty");
    }
    this->m_llmModel                = llama_model_load_from_file(modelPath.c_str(), model_params);
    if (this->m_llmModel == nullptr) {
        THROW_ERROR("error: unable to load model from %s" , modelPath.c_str());
    }
}

void LLM::LLMImpl::FreeModel()
{
    if (this->m_llmModel) {
        llama_model_free(this->m_llmModel);
        this->m_llmModel = nullptr;
    }
}

void LLM::LLMImpl::NewContext()
{
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx                = this->m_nCtx;
    ctx_params.n_threads            = this->m_config.GetConfigInt(LlmConfig::ConfigParam::NumThreads);
    ctx_params.n_threads_batch      = this->m_config.GetConfigInt(LlmConfig::ConfigParam::NumThreads);
    ctx_params.no_perf              = false;
    this->m_llmContext              = llama_init_from_model(this->m_llmModel, ctx_params);
    this->m_nCtx                    = llama_n_ctx(this->m_llmContext);
    this->m_batchSz                 = llama_n_batch(this->m_llmContext);
    LOG_INF("batch_size is %zu , context length is %d", this->m_batchSz, this->m_nCtx);
    if (this->m_llmContext == nullptr) {
        THROW_ERROR("NewContext failed: Unable to create llama context");
    }
}

void LLM::LLMImpl::FreeContext()
{
    if (this->m_llmContext) {
        llama_free(this->m_llmContext);
        this->m_llmContext = nullptr;
    }
}

void LLM::LLMImpl::llama_llm_log_callback(enum ggml_log_level level, const char * text, void * user_data) {
    // map the llama provided internal logs to LLM module style logs.
    switch (level) {
        case GGML_LOG_LEVEL_DEBUG:
            LOG_DEBUG("%s",text);
            break;
        case GGML_LOG_LEVEL_INFO:
            LOG_INF("%s",text);
            break;
        case GGML_LOG_LEVEL_WARN:
            LOG_WARN("%s",text);
            break;
        case GGML_LOG_LEVEL_ERROR:
            LOG_ERROR("%s",text);
            break;
            // logs with GGML_LOG_LEVEL 0 and 5 are irrelevant in llama.cpp
        default:
            break;
    }
    (void) level;
    (void) text;
    (void) user_data;
}

void LLM::LLMImpl::LlmInit(const LlmConfig& config, std::string sharedLibraryPath)
{
    try {
        llama_log_set(llama_llm_log_callback, nullptr);
        ggml_backend_load_all_from_path(sharedLibraryPath.c_str());
        this->m_config = config;
        this->m_batchSz = this->m_config.GetConfigInt(LlmConfig::ConfigParam::BatchSize);
        this->m_nCtx    = this->m_config.GetConfigInt(LlmConfig::ConfigParam::ContextSize);

        LoadModel();
        BackendInit();

        if (this->m_llmModel != nullptr) {
            NewContext();
        }
        NewSampler();
        this->m_llmInitialized = true;
    } catch (const std::exception& e) {
        THROW_ERROR("Llama model initialization failed: %s" ,e.what());
    }
    LOG_INF("Llama initialized successfully");
}

void LLM::LLMImpl::FreeLlm()
{
    if (this->m_llmInitialized) {
        FreeContext();
        FreeModel();
        BackendFree();
        this->m_nCur = 0;
        FreeSampler();
        this->m_llmInitialized = false;
        this->m_isConversationStart = true;
    }
}

void LLM::LLMImpl::BackendInit()
{
    llama_backend_init();
}

void LLM::LLMImpl::BackendFree()
{
    llama_backend_free();
}

void LLM::LLMImpl::FreeBatch()
{
    llama_batch_free(this->m_llmBatch);
}

void LLM::LLMImpl::FreeSampler()
{
    llama_sampler_free(this->m_pLlmSampler);
}

float LLM::LLMImpl::GetEncodeTimings()
{
    const auto resultsTiming = llama_perf_context(this->m_llmContext);
    return static_cast<float>(1e3 / resultsTiming.t_p_eval_ms * resultsTiming.n_p_eval);
}

float LLM::LLMImpl::GetDecodeTimings()
{
    const auto resultsTiming = llama_perf_context(this->m_llmContext);
    return static_cast<float>(1e3 / resultsTiming.t_eval_ms * resultsTiming.n_eval);
}

void LLM::LLMImpl::ResetTimings()
{
    llama_perf_context_reset(this->m_llmContext);
}

std::string LLM::LLMImpl::SystemInfo()
{
    return std::string(llama_print_system_info());
}

void LLM::LLMImpl::KVCacheClear()
{
    llama_memory_clear(llama_get_memory(this->m_llmContext), true);
}

void LLM::LLMImpl::KVCacheSeqRm(int32_t p0, int p1)
{
    // setting sequence ID to negative to match any sequence
    int seqId = -1;
    llama_memory_seq_rm(llama_get_memory(this->m_llmContext), seqId, p0, p1);
}

int32_t LLM::LLMImpl::GetInitialPromptLength(const char* text,
                                             int32_t textLength,
                                             bool addSpecial,
                                             bool parseSpecial)
{
    const llama_vocab* vocab = llama_model_get_vocab(this->m_llmModel);
    const auto tokens        = static_cast<llama_token *>(malloc(sizeof(llama_token) * this->m_nCtx));
    return llama_tokenize(vocab, text, textLength, tokens, this->m_nCtx, addSpecial, parseSpecial);
}

void LLM::LLMImpl::ResetContext()
{
    if (!this->m_systemPrompt.empty()) {
        auto n_prefix = GetInitialPromptLength(
            this->m_systemPrompt.c_str(), this->m_systemPrompt.length(), true, false);
        KVCacheSeqRm(n_prefix, -1);
        this->m_nCur = n_prefix;
    } else {
        KVCacheClear();
        this->m_nCur = 0;
        this->m_isConversationStart = true;
    }
}

llama_batch LLM::LLMImpl::NewBatch(int numTokens, int embeddings, int numSequenceMax)
{
    return llama_batch_init(numTokens, embeddings, numSequenceMax);
}

void LLM::LLMImpl::NewSampler()
{
    auto sparams        = llama_sampler_chain_default_params();
    sparams.no_perf     = false;
    this->m_pLlmSampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(this->m_pLlmSampler, llama_sampler_init_greedy());
}

bool LLM::LLMImpl::ApplyAutoChatTemplate(LlmChat::Payload& payload)
{
    const char* tmpl = llama_model_chat_template(this->m_llmModel, /*name*/ nullptr);

    // if no template on the model, fall back to default implementation
    if (!tmpl) {
        LOG_WARN("ApplyAutoChatTemplate: no template found. Falling back to default template.");
        return false;
    }

    std::vector<llama_chat_message> msgs;
    if (this->m_isConversationStart) {
        llama_chat_message sys{};
        sys.role    = "system";
        sys.content = this->m_systemPrompt.c_str();
        msgs.push_back(sys);
        this->m_isConversationStart = false;
    }

    llama_chat_message usr{};
    usr.role    = "user";
    usr.content = payload.textPrompt.c_str();
    msgs.push_back(usr);

    std::string templated;

    // initial call to determine the templated size before executing the actual chat template
    int32_t requiredMemory = llama_chat_apply_template(
        tmpl,
        msgs.data(),
        msgs.size(),
        /*add_assistant_prefix=*/true,
        templated.data(),
        static_cast<int>(templated.size())
    );

    if (requiredMemory < 0) {
        LOG_WARN("ApplyAutoChatTemplate failed. Falling back to default template.");
        return false;
    }

    templated.resize(requiredMemory);

    // apply chat template
    llama_chat_apply_template(
        tmpl,
        msgs.data(),
        msgs.size(),
        /*add_assistant_prefix=*/true,
        templated.data(),
        static_cast<int>(templated.size())
    );
    payload.textPrompt = templated;
    return true;
}

void LLM::LLMImpl::Encode(LlmChat::Payload& payload)
{
    const auto prompt_tokens = common_tokenize(this->m_llmContext, payload.textPrompt, true);

    size_t promptLength = prompt_tokens.size();
    // check prompt size
    if (promptLength + this->m_nCur > this->m_nCtx - 4) {
        const auto msg = "Failed to evaluate current prompt, context is full";
        THROW_ERROR("%s : %s", msg, __func__);
    }
    if (promptLength <= 1) {
        THROW_ERROR("%s : %s", "Prompt is empty.", __func__);
    }
    for (size_t idx = 0; idx < promptLength; idx += this->m_batchSz) {
        const size_t end_idx  = std::min(idx + this->m_batchSz, promptLength);
        const bool lastBatch = (end_idx == (promptLength));
        auto sub_prompt = std::vector<llama_token>(prompt_tokens.begin() + idx,
                                                   prompt_tokens.begin() + end_idx);
        if (!sub_prompt.empty()) {
            CompletionInit(sub_prompt, lastBatch);
        }
    }
}

void LLM::LLMImpl::CompletionInit(llama_tokens sub_tokens_list, bool lastBatch)
{
    // Synchronize llama to remove idle time between function calls
    llama_synchronize(this->m_llmContext);
    llama_batch batch = NewBatch(this->m_batchSz, 0, 1);
    common_batch_clear(batch);
    // evaluate the initial prompt
    for (auto i = this->m_nCur; i < sub_tokens_list.size() + this->m_nCur; i++) {
        common_batch_add(batch, sub_tokens_list[i - this->m_nCur], i, {0}, false);
    }

    // llama_decode will output logits only for the last token of the prompt
    if (lastBatch) {
        batch.logits[batch.n_tokens - 1] = true;
    }

    if (llama_decode(this->m_llmContext, batch) != 0) {
        THROW_ERROR("llama_decode(): Failed to evaluate prompt");
    }

    llama_synchronize(this->m_llmContext);
    this->m_nCur += batch.n_tokens;
}

std::string LLM::LLMImpl::CompletionLoop()
{
    const auto model =
            llama_get_model(this->m_llmContext); // CHANGE FROM JOBJECT TO PASSING ACTUAL CONTEXT

    const llama_vocab* vocab = llama_model_get_vocab(model);

    const auto new_token_id = llama_sampler_sample(this->m_pLlmSampler, this->m_llmContext, -1);

    if ((llama_vocab_eos(vocab) == new_token_id) || (this->m_nCur == this->m_nCtx)) {
        return this->m_eos;
    }

    auto new_token_chars = common_token_to_piece(this->m_llmContext, new_token_id);
    this->m_cachedTokenChars += new_token_chars;
    std::string new_token = "";
    if (is_utf8(this->m_cachedTokenChars.c_str(), this->m_cachedTokenChars.size())) {
        new_token = this->m_cachedTokenChars.c_str();
        this->m_cachedTokenChars.clear();
    } else {
        new_token = "";
    }
    llama_batch batch = NewBatch(this->m_batchSz, 0, 1);
    common_batch_clear(batch);
    common_batch_add(batch, new_token_id, this->m_nCur, {0}, true);

    if (llama_decode(this->m_llmContext, batch) != 0) {
        THROW_ERROR("llama_decode(): Failed to decode token");
    }
    // Synchronize llama to remove idle time between function calls
    llama_synchronize(this->m_llmContext);
    ++this->m_nCur;
    return new_token;
}

std::string LLM::LLMImpl::NextToken()
{
    std::string result = CompletionLoop();
    if ((result == this->m_eos) && (this->m_nCur >= this->m_nCtx)) {
        this->m_contextFilled = 100;
    } else {
        this->m_contextFilled = 100 * this->m_nCur / this->m_nCtx;
    }
    return result;
}

void LLM::LLMImpl::Cancel() {
	LOG_INF("Cancelling current operation");
}

size_t LLM::LLMImpl::GetChatProgress() const
{
    return this->m_contextFilled;
}

void LLM::LLMImpl::StopGeneration()
{
    // TODO: add stop response to support cancelled queries
}

std::string LLM::LLMImpl::GeneratePromptWithNumTokens(size_t numPromptTokens)
{
    if (numPromptTokens == 0) {
        return std::string{};
    }

    // Simple pattern that typically maps to a single token in LLaMA vocab
    // (space + capital letter works well for BPE-style tokenizers)
    const std::string pattern = " A";

    std::string prompt;
    std::vector<llama_token> tokens;

    // We’ll grow the prompt until tokenized length reaches or exceeds target.
    while (true) {
        prompt += pattern;
        tokens = common_tokenize(this->m_llmContext, prompt, /*add_bos=*/1);

        const size_t currentTokens = tokens.size();

        if (currentTokens == numPromptTokens) {
            return prompt;
        }

        if (currentTokens > numPromptTokens) {
            // We overshot. For now, return best-effort.
            return prompt;
        }
    }
}
