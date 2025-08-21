//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlamaVisionImpl.hpp"

#include <stdexcept>
#include <string>
#include <utility>

void LlamaVisionImpl::NewSampler() {
    // Set deterministic sampling parameters

    auto sampling = this->m_commonParams.sampling;
    sampling.temp  = 0.0f;
    sampling.top_p = 0.0f;
    sampling.top_k = 1;
    sampling.min_p = 0.0f;

    auto sampler = common_sampler_init(this->m_mtmdContext->model, sampling);
    if (!sampler) {
        throw std::runtime_error("NewSampler failed: sampler init returned null");
    }

    this->m_commonParams.sampling = sampling;
    this->m_commonSampler = sampler;

    if (!this->m_commonSampler) {
        throw std::runtime_error("New Sampler failed: sampler init returned null");
    }
}

void LlamaVisionImpl::FreeLlm() {
    if(this->m_commonSampler)
    {
        FreeSampler();
        this->m_commonSampler = nullptr;
    }

    this->m_mtmdContext.reset();
    this->m_llmContext = nullptr;
    this->m_llmModel   = nullptr;

    this->m_nCur = 0;
    this->m_contextFilled = 0;
    this->m_llmInitialized = false;
}

void LlamaVisionImpl::LlmInit(const LlmConfig& config) {
    if (config.GetNumThreads() <= 0) {
        throw std::invalid_argument("NumThreads must be > 0");
    }
    if (config.GetBatchSize() <= 0) {
        throw std::invalid_argument("BatchSize must be > 0");
    }

    try {
        this->m_config = config;
        this->m_batchSz = this->m_config.GetBatchSize();
        this->m_systemPrompt = this->m_config.GetSystemPrompt();
        this->m_isDefaultTemplate = this->m_config.IsDefaultTemplate();
        this->m_userTemplate = this->m_config.GetUserTemplate();
        this->m_systemTemplate = config.GetSystemTemplate();

        LoadModel();
        NewContext();
        NewSampler();
        this->m_llmInitialized = true;

    } catch (const std::exception& e) {
        throw std::runtime_error(
                std::string("Llama initialization failed: ") + e.what() + "\n"
        );
    }
}

void LlamaVisionImpl::ResetVisionContext() {
    this->m_mtmdContext->n_past = this->m_nCur;
    this->m_contextFilled = (100 * this->m_nCur) / this->m_nCtx;
    this->m_mtmdContext->bitmaps.entries.clear();
    this->m_imageIndex = 0;
    common_batch_clear(this->m_mtmdContext->batch);
    llama_perf_context_reset(this->m_llmContext);
    common_sampler_reset(this->m_commonSampler);
}

void LlamaVisionImpl::LoadModel()
{
    const auto& mmproj = this->m_config.GetMMPROJModelPath();
    const auto& model  = this->m_config.GetModelPath();

    if (mmproj.empty() || model.empty()) {
        throw std::invalid_argument("LoadModel error: modelPath or mmprojPath is empty");
    }

    // Just assign directly (keep it simple)
    this->m_commonParams.mmproj.path = mmproj;
    this->m_commonParams.model.path  = model;
}

void LlamaVisionImpl::Encode(const LLM::EncodePayload& payload) {
    llama_synchronize(this->m_llmContext);

    // 1) Load image into a local bitmaps container (only on first message)
    auto visionCtx = this->m_mtmdContext->ctx_vision.get();
    mtmd::bitmaps bitmaps;
    if (!payload.imagePath.empty()) {
        mtmd::bitmap bmp{ mtmd_helper_bitmap_init_from_file(visionCtx, payload.imagePath.c_str()) };
        bitmaps.entries.emplace_back(std::move(bmp));
    }

    // 2) Prepare text input
    const mtmd_input_text textInput{
            /* text          = */ payload.textPrompt.c_str(),
            /* add_special   = */ payload.isFirstMessage,
            /* parse_special = */ true
    };

    // 3) Tokenize text + image together
    auto bitmapData = bitmaps.c_ptr();
    mtmd::input_chunks chunks{ mtmd_input_chunks_init() };
    mtmd_tokenize(
            visionCtx,
            chunks.ptr.get(),
            &textInput,
            bitmapData.data(),
            bitmapData.size()
    );

    // 4) Clear any previously stored bitmaps in the context
    this->m_mtmdContext->bitmaps.entries.clear();

    // 5) Evaluate the chunks
    llama_pos newPast = 0;
    const bool evalFailed = mtmd_helper_eval_chunks(
            visionCtx,
            this->m_mtmdContext->lctx,
            chunks.ptr.get(),
            this->m_mtmdContext->n_past,
            /* offset = */ 0,
            this->m_mtmdContext->n_batch,
            /* reset_state = */ true,
            &newPast
    );

    if (evalFailed) {
        LOG_ERR("Failed to evaluate multimodal prompt\n");
        return;
    }

    llama_synchronize(this->m_llmContext);
    this->m_mtmdContext->n_past = newPast;
    this->m_nCur                = newPast;
    this->m_contextFilled = std::min<size_t>((100ULL * this->m_nCur) / this->m_nCtx, 100);
}

std::string LlamaVisionImpl::NextToken() {

    // Check if context is full before new token is being processed
    if (this->m_nCur  >= this->m_nCtx) {
        this->m_contextFilled = 100;
        return "ctx_full";
    }

    const auto token_id = common_sampler_sample(this->m_commonSampler, this->m_mtmdContext->lctx, -1);
    // Sample and accept the next token
    common_sampler_accept(this->m_commonSampler, token_id, true);

    // Prepare the batch for decoding
    common_batch_clear(this->m_mtmdContext->batch);
    common_batch_add(
            this->m_mtmdContext->batch,
            token_id,
            this->m_mtmdContext->n_past++,
            /* embedding = */ {0},
            /* skip      = */ true
    );

    // Decode and log any errors
    if (llama_decode(this->m_mtmdContext->lctx, this->m_mtmdContext->batch)) {
        LOG_ERR("Failed to decode token\n");
        return "";
    }

    ++this->m_nCur;
    llama_synchronize(this->m_llmContext);

    // Update fill to reflect the token we just processed
    this->m_contextFilled = std::min<size_t>((100ULL * this->m_nCur) / this->m_nCtx, 100);

    return common_token_to_piece(this->m_mtmdContext->lctx, token_id);
}

size_t LlamaVisionImpl::GetChatProgress() const {
    return this->m_contextFilled;
}

void LlamaVisionImpl::NewContext() {

    if (this->m_nCtx <= 0) {
        throw std::invalid_argument("Context length cannot be less than one");
    }

    auto params = this->m_commonParams;
    params.cpuparams.n_threads       = this->m_config.GetNumThreads();
    params.cpuparams_batch.n_threads = this->m_config.GetNumThreads();
    params.n_batch                   = this->m_batchSz;
    params.n_ctx                     = this->m_nCtx;

    auto ctx = std::make_unique<mtmd_app_context>(params);

    if (!ctx->ctx_vision) {
        throw std::runtime_error("NewContext failed: unable to create vision context");
    }
    if (!ctx->lctx) {
        throw std::runtime_error("NewContext failed: unable to create text context");
    }

    if (!ctx->model) {
        throw std::runtime_error("NewContext failed: unable to create text model");
    }

    this->m_commonParams   = params;
    this->m_mtmdContext    = std::move(ctx);
    this->m_llmContext     = this->m_mtmdContext->lctx;
    this->m_llmModel       = this->m_mtmdContext->model;
    this->m_nCur           = 0;
    this->m_contextFilled  = 0;

}
