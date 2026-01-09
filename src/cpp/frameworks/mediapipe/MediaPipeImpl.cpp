//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmImpl.hpp"
#include <string>
#include <iostream>
#include <filesystem>

#include "Logger.hpp"

// Enqueues a token into the queue if the supplied epoch matches the current one.
// Notifies one waiting consumer that new data may be available.
void TokenQueue::enqueue(uint64_t epoc, std::string token) {
    bool pushed = false;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (epoc == m_eosEpoch) {
            m_queue.push_back(std::move(token));
            m_numQueued++;
            pushed = true;
        }
    }

    if (pushed) {
        m_conditionVariable.notify_one();
    }
}


// Blocks until either a token becomes available for the current epoch
// or the epoch changes (via reset). Returns the next token on success,
// or an empty string if a reset was signaled while waiting.
std::string TokenQueue::dequeue() {
    std::unique_lock<std::mutex> lk(m_mutex);
    const uint64_t seen_epoch = m_eosEpoch; // snapshot before waiting

    m_conditionVariable.wait(lk, [&]{
        // Wake on either: new data or epoch change.
        return !m_queue.empty() || m_eosEpoch != seen_epoch;
    });

    // If the epoch changed while we waited, surface a reset signal.
    if (m_eosEpoch != seen_epoch) {
        return std::string(); // empty string indicates reset
    }

    std::string token = std::move(m_queue.front());
    m_queue.pop_front();
    return token;
}

// Clears all queued tokens, increments the epoch to invalidate any
// pending producers/consumers, wakes all waiters, and returns the new epoch.
uint64_t TokenQueue::reset() {
    uint64_t new_epoch;
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_queue.clear();
        m_numQueued = 0;
        new_epoch = ++m_eosEpoch;
    }
    m_conditionVariable.notify_all(); // wake all waiters; dequeue() will return ""
    return new_epoch;
}

// Returns true if the queue currently has no tokens.
bool TokenQueue::empty() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_queue.empty();
}

// Returns the number of tokens that have been queued since the last reset.
uint64_t TokenQueue::numQueued() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_numQueued;
}

// Returns the current epoch value used to distinguish queue lifetimes.
uint64_t TokenQueue::epoch() const {
    std::lock_guard<std::mutex> lk(m_mutex);
    return m_eosEpoch;
}


/**
  * LlmModelContext groups together metadata and shared state needed while
 * interacting with the language model, such as the current epoch/id,
 * the token queue used for streaming tokens, and the end-of-sequence marker.
 */
struct LlmModelContext {
    /**
     * @brief Logical epoch / generation identifier.
     *
     * Can be used to distinguish between different runs, requests, or
     * versions of the model state that share the same underlying queues.
     */
    uint64_t m_epoc;

    /**
     * @brief Reference to the token queue used to send or receive tokens.
     *
     * This queue typically carries tokens produced by the model or
     * consumed by downstream components (e.g., a streamer or client).
     * The context does not own the queue; it must remain valid for the
     * lifetime of this LlmModelContext.
     */
    TokenQueue& m_tokenQueue;

    /**
     * @brief End-of-sequence (EOS) marker for this context.
     *
     * When this string is produced or encountered, it usually signals that
     * the current generation should stop.
     */
    std::string m_eos;

    /**
     * @brief Constructs a new LlmModelContext.
     *
     * @param epoc        Logical epoch / generation identifier.
     * @param tokenQueue  Reference to the token queue associated with this context.
     * @param eos         End-of-sequence marker string.
     */
    explicit LlmModelContext(uint64_t epoc,
                             TokenQueue& tokenQueue,
                             std::string eos)
        : m_epoc(epoc),
          m_tokenQueue(tokenQueue),
          m_eos(std::move(eos)) { }
};

/**
 * @brief Mediapipe Implementation of our LLM API
 *
 */
LLM::LLMImpl::LLMImpl() = default;

LLM::LLMImpl::~LLMImpl()
{
    this->FreeLlm();
}

std::string GetCacheDir() {
    try {
        std::filesystem::path working_dir = std::filesystem::current_path();
        return working_dir.string();
    } catch (const std::filesystem::filesystem_error& e) {
        THROW_ERROR("Error getting current directory: %s" , e.what());
        return "";
    }
}

/**
 * Method to be registered as cpu_callback function inside Llm's Inference Engine after each token computation.
 * @param ctx This is a pointer to CallbackContext. We use the members of CallbackContext to synchronize token stream.
 * @param response_context Context for LLM to store information about each token
 */
void LlmCallback(void* ctx, LlmResponseContext* response_context) {

    auto llmModelContext = (LlmModelContext*) ctx;

    auto token = response_context->response_array[0];
    if ( std::strlen(token) > 0) {
        llmModelContext->m_tokenQueue.enqueue(llmModelContext->m_epoc,
                                              response_context->response_array[0]);
    }

    if (response_context->done) {
        llmModelContext->m_tokenQueue.enqueue(llmModelContext->m_epoc,
                                              llmModelContext->m_eos);
        delete llmModelContext;
    }
}

void LLM::LLMImpl::LoadEngine(const std::string& model_path, const std::string& cache_dir)
{
    const LlmModelSettings model_settings = {
        .model_path     = model_path.c_str(),
        .cache_dir      = cache_dir.c_str(),
        .max_num_tokens = this->m_nCtx,
        .num_threads   = static_cast<size_t>(this->m_config.GetConfigInt(LlmConfig::ConfigParam::NumThreads))
    };

    this->m_errorCode =
        LlmInferenceEngine_CreateEngine(&model_settings, &this->m_llmEngine, &this->m_errorMsg);
    if (this->m_errorCode) {
        LOG_ERROR("Failed to create engine: %s", this->m_errorMsg);
        free(this->m_errorMsg);
    }
}


void LLM::LLMImpl::LoadSession()
{
    const LlmSessionConfig session_config = {
        .topk        = 0,
        .topp        = 1.0f,
        .temperature = 0,
        .random_seed = this->m_randomSeed,
    };
    this->m_errorCode = LlmInferenceEngine_CreateSession(
        this->m_llmEngine, &session_config, &this->m_llmEngineSession, &this->m_errorMsg);
    if (this->m_errorCode) {
        LOG_ERROR("Failed to load session: %s", this->m_errorMsg);
        free(this->m_errorMsg);
    }
}

void LLM::LLMImpl::LlmInit(const LlmConfig& config, std::string sharedLibraryPath)
{
    try {
        this->m_config = config;
        const std::string modelPath = this->m_config.GetConfigString(LlmConfig::ConfigParam::LlmModelName);
        const std::string cache_dir = GetCacheDir();
        this->m_nCtx = this->m_config.GetConfigInt(LlmConfig::ConfigParam::ContextSize);

        std::filesystem::create_directories(cache_dir);
        LoadEngine(modelPath, cache_dir);

        if (this->m_errorCode) {
            THROW_ERROR("Mediapipe Engine creation failed");
        }

        LoadSession();
        if (this->m_errorCode) {
            THROW_ERROR("Mediapipe Session creation failed");
        }
        this->m_conversationContext = "";
        this->m_llmInitialized      = true;
    } catch (const std::exception& e) {
        THROW_ERROR("LLM initialization failed: %s" , e.what());
    }
    LOG_INF("Mediapipe Model initialized successfully");
}

void LLM::LLMImpl::Encode(LlmChat::Payload& payload)
{
    LOG_DEBUG("Sending in query %s\n", payload.textPrompt.c_str());

    std::string _query = this->m_conversationContext + payload.textPrompt;
    LOG_INF("Sending in query %s\n", _query.c_str());
    this->m_errorCode  = LlmInferenceEngine_Session_AddQueryChunk(
        this->m_llmEngineSession, _query.c_str(), &this->m_errorMsg);
    if (this->m_errorCode) {
        free(this->m_errorMsg);
        THROW_ERROR("Encode: Failed to evaluate: %s", this->m_errorMsg);
    }

    this->m_conversationContext = _query;
    auto nCur =  LlmInferenceEngine_Session_SizeInTokens(this->m_llmEngineSession,
                                                         _query.c_str(), &this->m_errorMsg);
    if (nCur < 0) {
        free(this->m_errorMsg);
        THROW_ERROR("Mediapipe Chat Progress Finder failed:");
        return;
    }
    if (nCur >= this->m_nCtx)
    {
        free(this->m_errorMsg);
        THROW_ERROR("LLM context is full");
        return;
    }

    auto epoc = m_tokenQueue.reset();
    auto context = new LlmModelContext(epoc, this->m_tokenQueue, this->m_eos);

    // The prediction can start as soon as we encode, extract the tokens with NextToken call.
    this->m_errorCode = LlmInferenceEngine_Session_PredictAsync(
    this->m_llmEngineSession, context, &this->m_errorMsg, LlmCallback);
    if (this->m_errorCode) {
        THROW_ERROR("Mediapipe predictAsync - Failed to decode token: %s", this->m_errorMsg);
    }
    this->m_conversationContext = _query;
}


std::string LLM::LLMImpl::NextToken()
{
    auto token = m_tokenQueue.dequeue();
    this->m_conversationContext += token;
    return token;
}


void LLM::LLMImpl::Cancel() {
    LOG_INF("Cancelling current operation and reseting context");
    
    this->StopGeneration();
    m_tokenQueue.reset();
}

float LLM::LLMImpl::GetEncodeTimings()
{
    return LlmInferenceEngine_Session_EncoderRate(this->m_llmEngineSession);
}

float LLM::LLMImpl::GetDecodeTimings()
{
    return LlmInferenceEngine_Session_DecoderRate(this->m_llmEngineSession);
}

void LLM::LLMImpl::ResetTimings()
{
    LlmInferenceEngine_Session_ResetTimings(this->m_llmEngineSession);
}

const char* LLM::LLMImpl::SystemInfo()
{
    return (char*)nullptr;
}

void LLM::LLMImpl::KVCacheClear()
{
    this->m_conversationContext.clear();
}

int32_t LLM::LLMImpl::GetInitialPromptLength(const char* text)
{
    auto nPrefix =
        LlmInferenceEngine_Session_SizeInTokens(this->m_llmEngineSession,
                                                text, &this->m_errorMsg);
    if (nPrefix < 0) {
        free(this->m_errorMsg);
        LOG_ERROR("Mediapipe Prefix Length Finder failed: %s", this->m_errorMsg);
        return -1;
    }
    return nPrefix;
}

void LLM::LLMImpl::ResetContext()
{
    if (!this->m_systemPrompt.empty()) {
        try {
            auto n_prefix= GetInitialPromptLength(this->m_systemPrompt.c_str());
            this->m_conversationContext = this->m_systemPrompt;
        } catch (const std::exception& e) {
            LOG_ERROR("Context reset failed: %s", e.what());
        }
    } else {
        KVCacheClear();

        this->m_conversationContext = "";
        this->m_isConversationStart = true;
    }
}

size_t LLM::LLMImpl::GetChatProgress()
{
     return (this->m_tokenQueue.numQueued() * 100) / this->m_nCtx;
}

void LLM::LLMImpl::FreeLlm()
{
    if (this->m_llmEngineSession) {
        LlmInferenceEngine_Session_Delete(this->m_llmEngineSession);
        this->m_llmEngineSession = nullptr;
    }

    if (this->m_llmEngine) {
        LlmInferenceEngine_Engine_Delete(this->m_llmEngine);
        this->m_llmEngine = nullptr;
    }
    this->m_isConversationStart = true;
}

void LLM::LLMImpl::StopGeneration()
{
     // Signal to cancel the response , helps in sending next query
     LlmInferenceEngine_Session_PendingProcessCancellation(
             static_cast<LlmInferenceEngine_Session*>(this->m_llmEngineSession),
             &this->m_errorMsg);
}

std::string LLM::LLMImpl::GeneratePromptWithNumTokens(size_t numPromptTokens)
{
    if (numPromptTokens == 0) {
        return std::string{};
    }

    // Simple base pattern; you can tweak this if needed.
    const std::string pattern = " A";

    std::string prompt;

    while (true) {
        prompt += pattern;

        // Ask the MediaPipe engine how many tokens this text would use
        int32_t n_tokens =
            LlmInferenceEngine_Session_SizeInTokens(this->m_llmEngineSession,
                                                    prompt.c_str(),
                                                    &this->m_errorMsg);

        if (n_tokens < 0) {
            // Error from engine
            std::string err = this->m_errorMsg ? this->m_errorMsg : "unknown error";
            if (this->m_errorMsg) {
                free(this->m_errorMsg);
                this->m_errorMsg = nullptr;
            }
            THROW_ERROR("Mediapipe benchmark: SizeInTokens failed: %s", err.c_str());
        }

        if (static_cast<size_t>(n_tokens) == numPromptTokens) {
            // Exact match – ideal case
            return prompt;
        }

        if (static_cast<size_t>(n_tokens) > numPromptTokens) {
            // We overshot. For now, return best-effort.
            return prompt;
        }
    }
}


