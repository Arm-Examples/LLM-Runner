//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmImpl.hpp"

#define LOG_INF(...)                  \
    do {                              \
        fprintf(stdout, __VA_ARGS__); \
    } while (0)

static bool is_valid_utf8(const char* string);

/**
 * @brief LLama Implementation of our LLM API
 *
 */
LLM::LLMImpl::LLMImpl() = default;

LLM::LLMImpl::~LLMImpl()
{
    this->FreeLlm();
}

void LLM::LLMImpl::LoadModel(const char* pathToModel)
{
    const llama_model_params model_params = llama_model_default_params();
    this->m_llmModel                = llama_model_load_from_file(pathToModel, model_params);
    if (this->m_llmModel == nullptr) {
        throw std::runtime_error("error: unable to load model from " + std::string(pathToModel));
    }
}

void LLM::LLMImpl::FreeModel()
{
    if (this->m_llmModel) {
        llama_model_free(this->m_llmModel);
        this->m_llmModel = nullptr;
    }
}

void LLM::LLMImpl::NewContext(int numThreads)
{
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx                = this->m_nCtx;
    ctx_params.n_threads            = numThreads;
    ctx_params.n_threads_batch      = numThreads;
    ctx_params.no_perf              = false;
    this->m_llmContext              = llama_init_from_model(this->m_llmModel, ctx_params);
    if (this->m_llmContext == nullptr) {
        throw std::runtime_error("NewContext failed: Unable to create llama context");
    }
}

void LLM::LLMImpl::FreeContext()
{
    if (this->m_llmContext) {
        llama_free(this->m_llmContext);
        this->m_llmContext = nullptr;
    }
}

void LLM::LLMImpl::LlmInit(const LlmConfig& config)
{
    try {
        this->m_batchSz = config.GetBatchSize();

        LoadModel(config.GetModelPath().c_str());
        BackendInit();

        this->m_llmPrefix = config.GetLlmPrefix();

        if (this->m_llmModel != nullptr) {
            NewContext(config.GetNumThreads());
        }
        NewSampler();
        this->m_llmInitialized = true;
    } catch (const std::exception& e) {
        throw std::runtime_error("Llama initialization failed: " + std::string(e.what()) + "/n");
    }
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
    llama_kv_self_clear(this->m_llmContext);
}

void LLM::LLMImpl::KVCacheSeqRm(int32_t p0, int p1)
{
    // setting sequence ID to negative to match any sequence
    int seqId = -1;
    llama_kv_self_seq_rm(this->m_llmContext, seqId, p0, p1);
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
    if (!this->m_llmPrefix.empty()) {
        auto n_prefix = GetInitialPromptLength(
            this->m_llmPrefix.c_str(), this->m_llmPrefix.length(), true, false);

        KVCacheSeqRm(n_prefix, -1);
        this->m_nCur = n_prefix;
    } else {
        KVCacheClear();
        this->m_nCur = 0;
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

void LLM::LLMImpl::Encode(std::string& prompt)
{
    const auto prompt_tokens = common_tokenize(this->m_llmContext, prompt, 1);

    size_t promptLength = prompt_tokens.size();

    // check prompt size
    if (promptLength > this->m_nCtx - 4) {
        fprintf(stderr, "%s: error: unable to Encode large prompt \n", __func__);
    } else if (promptLength + this->m_nCur > this->m_nCtx - 4) {
        fprintf(stdout, "%s: warning: unable to Encode prompt context full \n", __func__);
    } else if (promptLength <= 1) {
        fprintf(stderr, "%s: error: unable to Encode empty prompt \n", __func__);
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
        LOG_INF("llama_decode() failed");
        return;
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
        return "<|endoftext|>";
    }

    auto new_token_chars = common_token_to_piece(this->m_llmContext, new_token_id);
    this->m_cachedTokenChars += new_token_chars;
    std::string new_token = "";
    if (is_valid_utf8(this->m_cachedTokenChars.c_str())) {
        new_token = this->m_cachedTokenChars.c_str();
        this->m_cachedTokenChars.clear();
    } else {
        new_token = "";
    }
    llama_batch batch = NewBatch(this->m_batchSz, 0, 1);
    common_batch_clear(batch);
    common_batch_add(batch, new_token_id, this->m_nCur, {0}, true);

    if (llama_decode(this->m_llmContext, batch) != 0) {
        LOG_INF("llama_decode() failed");
    }

    // Synchronize llama to remove idle time between function calls
    llama_synchronize(this->m_llmContext);
    ++this->m_nCur;
    return new_token;
}

std::string LLM::LLMImpl::NextToken()
{
    std::string result = CompletionLoop();
    if ((result == "<|endoftext|>") && (this->m_nCur >= this->m_nCtx)) {
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
        LOG_INF("Benchmark prompt processing (pp)\n");

        common_batch_clear(this->m_llmBatch);

        const int n_tokens = prompts;
        for (i = 0; i < n_tokens; i++) {
            common_batch_add(this->m_llmBatch, 0, i, {0}, false);
        }

        this->m_llmBatch.logits[this->m_llmBatch.n_tokens - 1] = true;
        llama_kv_self_clear(this->m_llmContext);

        const auto t_prompts_start = ggml_time_us();
        if (llama_decode(this->m_llmContext, this->m_llmBatch) != 0) {
            LOG_INF("llama_decode() failed during prompt processing\n");
        }
        const auto t_prompts_end = ggml_time_us();

        // bench text generation

        LOG_INF("Benchmark text generation (tg)\n");

        llama_kv_self_clear(this->m_llmContext);
        const auto t_eval_prompts_start = ggml_time_us();
        for (i = 0; i < eval_prompts; i++) {
            common_batch_clear(this->m_llmBatch);
            for (int j = 0; j < n_max_sq; j++) {
                common_batch_add(this->m_llmBatch, 0, i, {j}, true);
            }

            LOG_INF("llama_decode() text generation: %d\n", i);
            if (llama_decode(this->m_llmContext, this->m_llmBatch) != 0) {
                LOG_INF("llama_decode() failed during text generation \n");
            }
        }

        const auto t_eval_prompts_end = ggml_time_us();

        llama_kv_self_clear(this->m_llmContext);

        const auto t_prompts      = static_cast<double>(t_prompts_end - t_prompts_start) / 1000000.0;
        const auto t_eval_prompts = static_cast<double>(t_eval_prompts_end - t_eval_prompts_start) / 1000000.0;

        const auto speed_prompts      = static_cast<double>(prompts) / t_prompts;
        const auto speed_eval_prompts = static_cast<double>(n_max_sq * eval_prompts) / t_eval_prompts;

        prompts_avg += speed_prompts;
        eval_prompts_avg += speed_eval_prompts;

        prompts_std += speed_prompts * speed_prompts;
        eval_prompts_std += speed_eval_prompts * speed_eval_prompts;

        LOG_INF("prompt eval %f t/s, token generation %f t/s\n", speed_prompts, speed_eval_prompts);
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

/**
 * @brief Checks if a given string is valid UTF-8.
 *
 * This function validates whether the input C-string adheres to the UTF-8 encoding standard.
 * It iterates through each byte of the string, determining the expected length of UTF-8 sequences
 * based on leading byte patterns, and verifies that subsequent bytes match the UTF-8 format.
 *
 * @param string Pointer to a null-terminated C-string to be validated.
 * @return true if the string is valid UTF-8 or if the input is a null pointer; false otherwise.
 */
static bool is_valid_utf8(const char* string)
{
    if (!string) {
        return true;
    }

    auto bytes = reinterpret_cast<const unsigned char *>(string);
    int num;

    while (*bytes != 0x00) {
        if ((*bytes & 0x80) == 0x00) {
            // U+0000 to U+007F
            num = 1;
        } else if ((*bytes & 0xE0) == 0xC0) {
            // U+0080 to U+07FF
            num = 2;
        } else if ((*bytes & 0xF0) == 0xE0) {
            // U+0800 to U+FFFF
            num = 3;
        } else if ((*bytes & 0xF8) == 0xF0) {
            // U+10000 to U+10FFFF
            num = 4;
        } else {
            return false;
        }

        bytes += 1;
        for (int i = 1; i < num; ++i) {
            if ((*bytes & 0xC0) != 0x80) {
                return false;
            }
            bytes += 1;
        }
    }
    return true;
}
