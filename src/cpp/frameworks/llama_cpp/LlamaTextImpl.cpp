//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
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
    const std::string& modelPath = this->m_config.GetConfigString("llmModelName");
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
    ctx_params.n_threads            = this->m_config.GetConfigInt("numThreads");
    ctx_params.n_threads_batch      = this->m_config.GetConfigInt("numThreads");
    ctx_params.no_perf              = false;
    this->m_llmContext              = llama_init_from_model(this->m_llmModel, ctx_params);
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
        case 1:
            LOG_DEBUG("%s",text);
            break;
        case 2:
            LOG_INF("%s",text);
            break;
        case 3:
            LOG_WARN("%s",text);
            break;
        case 4:
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
    ggml_backend_load_all_from_path(sharedLibraryPath.c_str());
    try {
        llama_log_set(llama_llm_log_callback, nullptr);
        this->m_config = config;
        this->m_batchSz = this->m_config.GetConfigInt("batchSize");
        this->m_nCtx    = this->m_config.GetConfigInt("contextSize");

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
        LOG_INF("ApplyAutoChatTemplate: no template found. Falling back to default template.");
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
        LOG_INF("ApplyAutoChatTemplate failed. Falling back to default template.");
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
    const auto prompt_tokens = common_tokenize(this->m_llmContext, payload.textPrompt, 1);

    size_t promptLength = prompt_tokens.size();
    const char * msg = "Failed to evaluate current prompt, context is full";
    // check prompt size
    if (promptLength > this->m_nCtx - 4) {
        THROW_ERROR("%s: Failed to evaluate large prompt",__func__);
    }
    else if (promptLength + this->m_nCur > this->m_nCtx - 4) {
        THROW_ERROR("%s : %s",msg,__func__);
    }
    else if (promptLength <= 1) {
        THROW_ERROR("%s : %s",msg,__func__);
    } else {
        for (size_t idx = 0; idx < promptLength; idx += this->m_batchSz) {
            const size_t end_idx  = std::min(idx + this->m_batchSz, promptLength - 1);
            const bool lastBatch = (end_idx == (promptLength - 1));
            auto sub_prompt = std::vector<llama_token>(prompt_tokens.begin() + idx,
                                                       prompt_tokens.begin() + end_idx + 1);
            if (!sub_prompt.empty()) {
                CompletionInit(sub_prompt, lastBatch);
            }
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
        return "ctx_full";
    } else {
        this->m_contextFilled = 100 * this->m_nCur / this->m_nCtx;
    }
    return result;
}

size_t LLM::LLMImpl::GetChatProgress() const
{
    return this->m_contextFilled;
}

std::string LLM::LLMImpl::BenchModel(int& prompts, int& eval_prompts, int& n_max_sq, int& n_rep)
{
    auto prompts_avg      = 0.0;
    auto eval_prompts_avg = 0.0;
    auto prompts_std      = 0.0;
    auto eval_prompts_std = 0.0;

    LOG_INF("m_nCtx = %d", this->m_nCtx);

    int i;
    for (int nri = 0; nri < n_rep; nri++) {
        LOG_INF("Benchmark prompt processing (pp)");

        common_batch_clear(this->m_llmBatch);

        const int n_tokens = prompts;
        for (i = 0; i < n_tokens; i++) {
            common_batch_add(this->m_llmBatch, 0, i, {0}, false);
        }

        this->m_llmBatch.logits[this->m_llmBatch.n_tokens - 1] = true;
        llama_memory_clear(llama_get_memory(this->m_llmContext), true);

        const auto t_prompts_start = ggml_time_us();
        if (llama_decode(this->m_llmContext, this->m_llmBatch) != 0) {
            LOG_INF("llama_decode() failed during prompt processing");
        }
        const auto t_prompts_end = ggml_time_us();

        // bench text generation

        LOG_INF("Benchmark text generation (tg)");

        llama_memory_clear(llama_get_memory(this->m_llmContext), true);
        const auto t_eval_prompts_start = ggml_time_us();
        for (i = 0; i < eval_prompts; i++) {
            common_batch_clear(this->m_llmBatch);
            for (int j = 0; j < n_max_sq; j++) {
                common_batch_add(this->m_llmBatch, 0, i, {j}, true);
            }

            LOG_INF("llama_decode() text generation: %d", i);
            if (llama_decode(this->m_llmContext, this->m_llmBatch) != 0) {

                THROW_ERROR("llama_decode() failed during text generation ");
            }
        }

        const auto t_eval_prompts_end = ggml_time_us();

        llama_memory_clear(llama_get_memory(this->m_llmContext), true);

        const auto t_prompts      = static_cast<double>(t_prompts_end - t_prompts_start) / 1000000.0;
        const auto t_eval_prompts = static_cast<double>(t_eval_prompts_end - t_eval_prompts_start) / 1000000.0;

        const auto speed_prompts      = static_cast<double>(prompts) / t_prompts;
        const auto speed_eval_prompts = static_cast<double>(n_max_sq * eval_prompts) / t_eval_prompts;

        prompts_avg += speed_prompts;
        eval_prompts_avg += speed_eval_prompts;

        prompts_std += speed_prompts * speed_prompts;
        eval_prompts_std += speed_eval_prompts * speed_eval_prompts;

        LOG_INF("prompt eval %f t/s, token generation %f t/s", speed_prompts, speed_eval_prompts);
    }

    prompts_avg /= static_cast<double>(n_rep);
    eval_prompts_avg /= static_cast<double>(n_rep);

    if (n_rep > 1) {
        prompts_std = sqrt(prompts_std / static_cast<double>(n_rep - 1) -
                           prompts_avg * prompts_avg * static_cast<double>(n_rep) / static_cast<double>(n_rep - 1));
        eval_prompts_std =
                sqrt(eval_prompts_std / static_cast<double>(n_rep - 1) -
                     eval_prompts_avg * eval_prompts_avg * static_cast<double>(n_rep) / static_cast<double>(n_rep - 1));
    } else {
        prompts_std      = 0;
        eval_prompts_std = 0;
    }

    char model_desc[128];
    llama_model_desc(this->m_llmModel, model_desc, sizeof(model_desc));

    const auto model_size = static_cast<double>(llama_model_size(this->m_llmModel)) / 1024.0 / 1024.0 / 1024.0;
    const auto model_n_params = static_cast<double>(llama_model_n_params(this->m_llmModel)) / 1e9;

    const auto backend = "cpu"; // TODO: What should this be?

    std::stringstream result;
    result << "| model | size | params | backend | test | t/s |\n";
    result << "| --- | --- | --- | --- | --- | --- |\n";
    result << "| " << model_desc << " | " << model_size << "GiB | " << model_n_params << "B | "
           << backend << " | prompts " << prompts << " | " << prompts_avg << " ± " << prompts_std
           << " |\n";
    result << "| " << model_desc << " | " << model_size << "GiB | " << model_n_params << "B | "
           << backend << " | tg " << eval_prompts << " | " << eval_prompts_avg << " ± "
           << eval_prompts_std << " |\n";

    return result.str().c_str();
}

void LLM::LLMImpl::StopGeneration()
{
    // TODO: add stop response to support cancelled queries
}

