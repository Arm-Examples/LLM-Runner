//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlamaVisionImpl.hpp"
#include "Logger.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include "ggml-backend.h"

void LlamaVisionImpl::NewSampler() {
    // Set deterministic sampling parameters

    auto sampling = this->m_commonParams.sampling;
    sampling.temp  = 0.0f;
    sampling.top_p = 0.0f;
    sampling.top_k = 1;
    sampling.min_p = 0.0f;

    auto sampler = common_sampler_init(this->m_mtmdContext->model, sampling);
    if (!sampler) {
        THROW_ERROR("NewSampler failed: sampler init returned null");
    }

    this->m_commonParams.sampling = sampling;
    this->m_commonSampler = sampler;

    if (!this->m_commonSampler) {
        THROW_ERROR("New Sampler failed: sampler init returned null");
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
    this->m_allocated = 0;
    this->m_llmInitialized = false;
}

void LlamaVisionImpl::LlmInit(const LlmConfig& config, std::string sharedLibraryPath) {
    if (config.GetConfigInt(LlmConfig::ConfigParam::NumThreads) <= 0) {
        THROW_INVALID_ARGUMENT("NumThreads must be > 0");
    }
    if (config.GetConfigInt(LlmConfig::ConfigParam::BatchSize) <= 0) {
        THROW_INVALID_ARGUMENT("BatchSize must be > 0");
    }
    if (config.GetConfigInt(LlmConfig::ConfigParam::ContextSize) <= 0) {
        THROW_INVALID_ARGUMENT("contextSize must be > 0");
    }


    try {
        // llama_log_set should be the very beginning of llama initialization to obtain all logs.
        llama_log_set(llama_llm_log_callback, nullptr);
        ggml_backend_load_all_from_path(sharedLibraryPath.c_str());
        this->m_config = config;
        this->m_batchSz = this->m_config.GetConfigInt(LlmConfig::ConfigParam::BatchSize);
        this->m_nCtx    = this->m_config.GetConfigInt(LlmConfig::ConfigParam::ContextSize);

        LoadModel();
        NewContext();
        NewSampler();
        this->m_llmInitialized = true;

    } catch (const std::exception& e) {
        THROW_ERROR("Llama initialization failed: %s",  e.what());
    }
    LOG_INF("Llama MMTD-Model initialized successfully");
}

void LlamaVisionImpl::ResetVisionContext() {
    this->m_mtmdContext->n_past = this->m_nCur;
    this->m_contextFilled = (100 * this->m_nCur) / this->m_nCtx;
    this->m_mtmdContext->bitmaps.entries.clear();
    this->m_imageIndex = 0;
    this->m_allocated = 0;
    common_batch_clear(this->m_mtmdContext->batch);
    llama_perf_context_reset(this->m_llmContext);
    common_sampler_reset(this->m_commonSampler);
}

void LlamaVisionImpl::LoadModel()
{
    const auto& mmproj = this->m_config.GetConfigString(LlmConfig::ConfigParam::ProjModelName);
    const auto& model  = this->m_config.GetConfigString(LlmConfig::ConfigParam::LlmModelName);

    if (mmproj.empty() || model.empty()) {
        THROW_INVALID_ARGUMENT("LoadModel error: modelPath or mmprojPath is empty");
    }

    // Just assign directly (keep it simple)
    this->m_commonParams.mmproj.path = mmproj;
    this->m_commonParams.model.path  = model;
}

void LlamaVisionImpl::Encode(LlmChat::Payload& payload) {
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

    if (this->m_allocated + mtmd_helper_get_n_tokens(chunks.ptr.get()) >= this->m_nCtx) {
        THROW_ERROR("Encode: Failed to evaluate since context is full" );
    }
    //clip_n_output_tokens

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
        THROW_ERROR("Encode: Failed to evaluate multimodal prompt");
    }

    llama_synchronize(this->m_llmContext);
    this->m_mtmdContext->n_past = newPast;
    this->m_nCur                = newPast;

    this->m_allocated += mtmd_helper_get_n_tokens(chunks.ptr.get());
    this->m_contextFilled = std::min<size_t>((100ULL * this->m_nCur) / this->m_nCtx, 100);

}

std::string LlamaVisionImpl::NextToken() {

    // Check if context is full before new token is being processed
    if (this->m_nCur  >= this->m_nCtx) {
        this->m_contextFilled = 100;
        return this->m_eos;
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
        THROW_ERROR("Failed to decode token");
        return "";
    }

    ++this->m_nCur;
    ++this->m_allocated;
    llama_synchronize(this->m_llmContext);

    // Update fill to reflect the token we just processed
    this->m_contextFilled = std::min<size_t>((100ULL * this->m_nCur) / this->m_nCtx, 100);

    return common_token_to_piece(this->m_mtmdContext->lctx, token_id);
}

void LlamaVisionImpl::Cancel() {
    LOG_INF("Cancelling current operation and context");
}

size_t LlamaVisionImpl::GetChatProgress() const {
    return this->m_contextFilled;
}

void LlamaVisionImpl::NewContext() {

    if (this->m_nCtx <= 0) {
        THROW_INVALID_ARGUMENT("Context length cannot be less than one");
    }

    auto params = this->m_commonParams;
    params.cpuparams.n_threads       = this->m_config.GetConfigInt(LlmConfig::ConfigParam::NumThreads);
    params.cpuparams_batch.n_threads = this->m_config.GetConfigInt(LlmConfig::ConfigParam::NumThreads);
    params.n_batch                   = std::min<size_t>(this->m_batchSz,this->m_nCtx);
    params.n_ctx                     = this->m_nCtx;

    auto ctx = std::make_unique<mtmd_app_context>(params);

    if (!ctx->ctx_vision) {
        THROW_ERROR("NewContext failed: unable to create vision context");
    }
    if (!ctx->lctx) {
        THROW_ERROR("NewContext failed: unable to create text context");
    }

    if (!ctx->model) {
        THROW_ERROR("NewContext failed: unable to create text model");
    }

    this->m_commonParams   = params;
    this->m_mtmdContext    = std::move(ctx);
    this->m_llmContext     = this->m_mtmdContext->lctx;
    this->m_llmModel       = this->m_mtmdContext->model;
    this->m_nCur           = 0;
    this->m_contextFilled  = 0;
    this->m_allocated      = 0;
    this->m_nCtx           = llama_n_ctx(this->m_llmContext);
    this->m_batchSz        = llama_n_batch(this->m_llmContext);

}
