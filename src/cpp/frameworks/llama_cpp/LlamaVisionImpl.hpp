//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef ARM_LLM_WRAPPER_LLAMAVISIONIMPL_HPP
#define ARM_LLM_WRAPPER_LLAMAVISIONIMPL_HPP

#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "mtmd.h"
#include "mtmd-helper.h"
#include "sampling.h"
#include "LlamaTextImpl.hpp"
#include "Logger.hpp"

/**
 * @brief Application context for multimodal (text + vision) inference using llama.cpp and MTMD.
 *
 * This struct owns the lifecycle of a llama model and its associated vision context.
 * It bundles together the model, vocabulary, evaluation context, batching state,
 * and MTMD vision bitmaps. It is constructed from common_params, which
 * specifies model paths and threading options.
 */
struct mtmd_app_context {
    /** Vision model context (CLIP / multimodal encoder). */
    mtmd::context_ptr ctx_vision{};

    /** Result of llama initialization (model + context handles). */
    common_init_result llama_init{};

    /** Pointer to the loaded llama model. */
    llama_model* model = nullptr;

    /** Llama evaluation context. */
    llama_context* lctx = nullptr;

    /** Model vocabulary. */
    const llama_vocab* vocab = nullptr;

    /** Batch object for tokenized inputs. */
    llama_batch batch{};

    /** Number of tokens per batch. */
    int n_batch = 0;

    /** Container for input bitmaps (images). */
    mtmd::bitmaps bitmaps{};

    /** Number of CPU threads to use. */
    int n_threads = 1;

    /** Current past token position (used for autoregressive decoding). */
    llama_pos n_past = 0;

    /**
     * @brief Construct a new application context from parameters.
     * @param params Common initialization parameters for llama + MTMD.
     */
    explicit mtmd_app_context(common_params& params)
            : llama_init(common_init_from_params(params)),
              model(llama_init.model.get()),
              lctx(llama_init.context.get()),
              vocab(llama_model_get_vocab(model)),
              n_threads(params.cpuparams.n_threads),
              batch(llama_batch_init(params.n_batch, 0, 1)),
              n_batch(params.n_batch) {
        if (!lctx) {
            THROW_ERROR("Error initialising multimodal app context. "
                                                 "llama_context is null.");
        }
        if (!model) {
            THROW_ERROR("Error initialising multimodal app context. "
                                                 "llama_model is null.");
        }

        init_vision_context(params);
    }

    /**
     * @brief Destructor to free batch resources.
     */
    ~mtmd_app_context() {
        llama_batch_free(batch);
    }

    /**
     * @brief Initialize the MTMD vision context.
     * @param params Common initialization parameters including mmproj path and GPU/CPU settings.
     */
    void init_vision_context(const common_params& params) {
        const std::string& clip_path = params.mmproj.path;

        auto mparams       = mtmd_context_params_default();
        mparams.use_gpu    = params.mmproj_use_gpu;
        mparams.print_timings = true;
        mparams.n_threads  = params.cpuparams.n_threads;
        //we are mapping LLM_LOG_LEVEL to GGML_LOG_LEVEL
        std::unordered_map<int,ggml_log_level> log_mapping{
            {0,GGML_LOG_LEVEL_ERROR},
            {1,GGML_LOG_LEVEL_WARN},
            {2,GGML_LOG_LEVEL_INFO},
            {3,GGML_LOG_LEVEL_DEBUG},
            {4,GGML_LOG_LEVEL_NONE}
        };
        mparams.verbosity = log_mapping[ACTIVE_LOG_LEVEL];
        ctx_vision.reset(mtmd_init_from_file(clip_path.c_str(), model, mparams));
        if (!ctx_vision) {
            THROW_ERROR("Failed to load vision model from %s", clip_path.c_str());
        }
    }
};

/**
 * @brief Llama multimodal (text + vision) wrapper implementation.
 */
class LlamaVisionImpl : public LLM::LLMImpl {
public:
    using LLM::LLMImpl::LLMImpl;  ///< Inherit base constructors
    ~LlamaVisionImpl() = default;

    /**
     * @brief Deleted copy constructor.
     */
    LlamaVisionImpl(const LlamaVisionImpl&) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    LlamaVisionImpl& operator=(const LlamaVisionImpl&) = delete;

    /**
     * @brief Move constructor.
     */
    LlamaVisionImpl(LlamaVisionImpl&&) noexcept = default;

    /**
     * @brief Move assignment operator.
     * @return Reference to this instance.
     */
    LlamaVisionImpl& operator=(LlamaVisionImpl&&) noexcept = default;

    /**
     * @brief Initialize the LLM with configuration parameters.
     * @param config Configuration for model, vision projection, and threading.
     * @param sharedLibraryPath path to location of shared libs
     */
    void LlmInit(const LlmConfig& config, std::string sharedLibraryPath = "") override;

    /**
     * @brief Reset both the LLM text context and the vision context state.
     */
    void ResetContext() override {
        LLM::LLMImpl::ResetContext();
        ResetVisionContext();
    }

    /**
     * @brief Sample and return the next token from the model.
     * @return The decoded token as a string.
     */
    std::string NextToken() override;

    /**
     * @brief Report current chat progress as a percentage of context used.
     * @return Progress in the range [0, 100].
     */
    size_t GetChatProgress() const override;

    /**
     * @brief Encode a multimodal payload (text + optional image).
     * @param payload Input payload containing text and/or image path.
     */
    void Encode(LlmChat::Payload& payload) override;

    /**
     * @brief Load the llama model from the given configuration.
     */
    void LoadModel() override;

    /**
     * @brief Create a new runtime context (model + vision).
     */
    void NewContext() override;

    /**
     * @brief List supported input modalities.
     * @return A vector containing {"text", "vision"}.
     */
    std::vector<std::string> SupportedInputModalities() const override {
        static const std::vector<std::string> kModalities = {"text", "image"};
        return kModalities;
    }

    /**
     * Method to Free all allocations pertaining to llama model
     */
    void FreeLlm() override;

    /**
     * Function to create a new sampler object in memory
     */
    void NewSampler() override;

    /**
     * Adjusts the prompt with the media tag needed by multimodal encode
     * @param payload The input payload containing the user's text prompt and the image path to apply the template to.
     */
    void QueryBuilder(LlmChat::Payload& payload) override {
        if(payload.imagePath != "") {
            payload.textPrompt = payload.textPrompt + "#" + std::to_string(this->m_imageIndex) + this->m_mediaMarker;
            this->m_imageIndex += 1;
        }
        LlmChat::QueryBuilder(payload);
    }

private:
    /** MTMD + llama application context. */
    std::unique_ptr<mtmd_app_context> m_mtmdContext{nullptr};

    /** Common model parameters. */
    common_params m_commonParams{};

    /** Sampler used for decoding tokens. */
    common_sampler* m_commonSampler{nullptr};

    /** Length of prefix tokens encoded. */
    size_t m_prefixLength{0};

    /** Image index in the conversation */
    size_t m_imageIndex{0};

    /** Resets the vision-specific application context. */
    void ResetVisionContext();

    /** Media marker which llama mtmd needs to tokenize the image */
    const std::string m_mediaMarker = mtmd_default_marker();
};

#endif // ARM_LLM_WRAPPER_LLAMAVISIONIMPL_HPP
