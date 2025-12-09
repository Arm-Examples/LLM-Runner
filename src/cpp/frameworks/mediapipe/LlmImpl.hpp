//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef LLM_IMPL_HPP
#define LLM_IMPL_HPP

#include "Llm.hpp"
#include "LlmConfig.hpp"
#include "llm_inference_engine.h"
#include <string>
#include <mutex>
#include <deque>
#include <condition_variable>


/**
 * @class TokenQueue
 * @brief A thread-safe queue of string tokens gated by an epoch value.
 *
 * This queue supports multiple producers and consumers. Tokens are only accepted
 * into the queue if the provided epoch matches the current queue epoch.
 * Consumers block in @ref dequeue until either a token for the current epoch
 * arrives or the epoch changes (in which case an empty string is returned to
 * signal a reset).
 *
 * Typical usage:
 * @code
 * TokenQueue q;
 * auto e = q.epoch();           // get current epoch
 * q.enqueue(e, "hello");        // accepted
 * std::string t = q.dequeue();  // -> "hello"
 *
 * e = q.reset();                // bump epoch, clear pending tokens
 * q.enqueue(e-1, "stale");      // ignored (stale epoch)
 * std::string t2 = q.dequeue(); // blocks until a token for epoch e arrives,
 *                               // or returns "" if epoch changes again
 * @endcode
 *
 * @note All public member functions are thread-safe.
 * @note Link with threads support, e.g. `-pthread` on GCC/Clang.
 */
class TokenQueue {
public:
    /// @brief Construct an empty queue with initial epoch 0.
    TokenQueue() = default;

    /// @brief Default destructor.
    ~TokenQueue() = default;

    // Non-copyable / non-movable (m_mutex/condvar members make this awkward).
    TokenQueue(const TokenQueue&) = delete;            ///< @private
    TokenQueue& operator=(const TokenQueue&) = delete; ///< @private
    TokenQueue(TokenQueue&&) = delete;                 ///< @private
    TokenQueue& operator=(TokenQueue&&) = delete;      ///< @private

    /**
     * @brief Enqueue a token iff the provided epoch matches the current epoch.
     *
     * If @p epoc does not match the current epoch, the token is discarded.
     * On successful push, one waiting consumer is notified.
     *
     * @param epoc Epoch to validate against the current queue epoch.
     * @param token The token to enqueue (moved if accepted).
     *
     * @post If @p epoc equals the current epoch, queue size increases by 1.
     * @post If accepted, a waiting consumer is notified.
     *
     * thread_safety Safe to call concurrently with other member functions.
     */
    void enqueue(uint64_t epoc, std::string token);

    /**
     * @brief Dequeue a token for the current epoch, waiting if necessary.
     *
     * Blocks until either:
     *  - a token is available for the current epoch; or
     *  - the epoch changes via @ref reset (or otherwise).
     *
     * If the epoch changes while waiting, returns an empty string to indicate
     * that consumers should refresh their view of the epoch and possibly
     * restart their logic.
     *
     * @return The next token (when available) or an empty string if the epoch
     *         changed while waiting.
     *
     * thread_safety Safe to call concurrently with other member functions.
 */
    std::string dequeue();

    /**
     * @brief Bump the epoch, clear any queued tokens, and notify all waiters.
     *
     * Increments the internal epoch, clears the pending queue, and wakes all
     * waiting consumers (which will cause @ref dequeue to return an empty
     * string if they were waiting on the previous epoch).
     *
     * @return The new epoch value after increment.
     *
     * @post Queue is empty.
     * @post All waiting consumers are notified.
     *
     * thread_safety Safe to call concurrently with other member functions.
     */
    uint64_t reset();

    /**
       * @brief returns the number of total tokens queued for this epoc
       *
       * @return  returns the number of total token queued on this epoc
       *
       * thread_safety Safe to call concurrently with other member functions.
       */
    uint64_t numQueued() const;
    /**
     * @brief Check whether the queue is empty.
     * @return @c true if empty, @c false otherwise.
     *
     * thread_safety Safe to call concurrently with other member functions.
     */
    bool empty() const;

    /**
     * @brief Get the current epoch value.
     * @return The current epoch.
     *
     * thread_safety Safe to call concurrently with other member functions.
     */
    uint64_t epoch() const;

private:
    mutable std::mutex m_mutex;                ///< Protects all members below.
    std::condition_variable m_conditionVariable;          ///< Signals enqueue/reset events.
    std::deque<std::string> m_queue;           ///< FIFO of tokens for current epoch.
    uint64_t m_eosEpoch = 0;              ///< Current epoch (starts at 0).
    uint64_t m_numQueued = 0;              ///< Current epoch (starts at 0).

};


/**
 * @brief Mediapipe Implementation of our LLM API
 */
class LLM::LLMImpl : public LlmChat {

public:
    LLMImpl();
    ~LLMImpl();
    /**
     * Initializes the LLM engine and session with configuration settings.
     * @param config Configuration object containing model path and prefix.
     * @param sharedLibraryPath path to location of shared libs
     */
    void LlmInit(const LlmConfig& config, std::string sharedLibraryPath = "");

    /**
     * Encodes a query and appends it to the current conversation context.
     * Adds the query chunk to the LLM engine session.
     * @param payload Input payload containing text and/or image path.
     */
    void Encode(LlmChat::Payload& payload);

    /**
     * Method to extract tokens already generated by LLM engine from Mediapipe Framework.
     * @return response
     */
    std::string NextToken();
 
    /**
    * Method to request the cancellation of a ongoing operation / functional call
    */
    void Cancel();

    /**
     * @brief Enqueue a token iff the provided epoch matches the current epoch.
     *
     * If @p epoc does not match the current epoch, the token is discarded.
     * On successful push, one waiting consumer is notified.
     *
     * @param epoc Epoch to validate against the current queue epoch.
     * @param token The token to enqueue (moved if accepted).
     *
     * @post If @p epoc equals the current epoch, queue size increases by 1.
     * @post If accepted, a waiting consumer is notified.
     *
     */
    void enqueueToken(uint64_t epoc, std::string& token);

    /**
     * Function to retrieve the mediapipe encode timings.
     * @return The encoded tokens per second
     */
    float GetEncodeTimings();

    /**
     * Function to retrieve the mediapipe decode timings.
     * @return The decoded tokens per second
     */
    float GetDecodeTimings();

    /**
     * Function to reset the mediapipe encode/decode timings
     */
    void ResetTimings();

    /**
     * Function to print the system info
     * @return System info as a char pointer
     */
    const char* SystemInfo();

    /**
     * Function to clear KV Cache and hence all conversation history
     */
    void KVCacheClear();

    /**
     * Method to reset conversation history and preserve encoded system prompt.
     * If model's prefix is not defined all conversation history would be cleared
     */
    void ResetContext();

    /**
     * Calculates and returns the percentage of context used in the chat.
     * @return Chat progress as a percentage of total context.
     */
    size_t GetChatProgress();

    /**
     * Frees the memory used by the LLM engine and session.
     * Cleans up allocated resources.
     */
    void FreeLlm();

    /**
     * Method to get framework type
     * @return string framework type
     */
    static std::string GetFrameworkType() {return "mediapipe";}

    /**
    * Method to Cancel generation of response tokens. Can be used to stop response once query commences
    */
    void StopGeneration();

    /**
    * @brief List supported input modalities.
    * @return A vector containing {"text", "vision"}.
    */
    std::vector<std::string> SupportedInputModalities() const {  return {"text"};}

    /**
     * Applies the automatic chat template to the given prompt.
     * @param payload The input prompt to apply the template to.
     * @return The prompt with the automatic chat template applied.
     */
    bool ApplyAutoChatTemplate(LlmChat::Payload& payload) override { return false;}

    /**
     * @brief Creates a synthetic text prompt that tokenizes to the given size.
     *
     * Used for benchmarking to ensure the encode phase receives a fixed
     * number of input tokens.
     *
     * @param numPromptTokens Desired number of input tokens.
     * @return A text prompt that produces that many tokens when encoded.
     */
    std::string GeneratePromptWithNumTokens(size_t numPromptTokens);

private:
    // Pointer to underlying LLM engine instance
    void* m_llmEngine{nullptr};
    // Last error message (if any)
    char* m_errorMsg{nullptr};
    // Active LLM engine session handle
    void* m_llmEngineSession{nullptr};
    // Context window size (number of tokens)
    size_t m_nCtx{0};
    // Seed for random number generation
    size_t m_randomSeed{1234};
    // Flag indicating LLM initialization status
    bool m_llmInitialized{false};
    // Last error code
    int m_errorCode{-1};
    // Accumulated conversation context
    std::string m_conversationContext;
    // Structure holding response metadata
    LlmResponseContext m_llmResponseContext;
    // Configuration for model
    LlmConfig m_config;

    // Used as a general signal in our LLM module to terminate response
    std::string m_eos = "<|endoftext|>";

    /**
     * Function to tokenize the initial prompt
     * @param text - prompt text
     * @return number of tokens in the prompt.
     */
    int32_t GetInitialPromptLength(const char* text);

    /**
     * Loads the LLM engine from the provided model path and cache directory.
     * Initializes the engine with the given settings.
     * @param model_path Path to the LLM model file.
     * @param cache_dir Directory to use for caching intermediate artifacts.
     */
    void LoadEngine(const std::string& model_path, const std::string& cache_dir = "./");

    /**
     * Creates a session for the LLM engine using preset hyperparameters.
     * Initializes the session handle for inference.
     */
    void LoadSession();

    /**
     * Stores tokens retrieved from the model in a thread safe Queue
    */
    TokenQueue m_tokenQueue = TokenQueue();
};

#endif /* LLM_IMPL_HPP */
