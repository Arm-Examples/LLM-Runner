//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmImpl.hpp"

#include <chrono>
#include <cinttypes>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <executorch/extension/llm/runner/irunner.h>
#include <executorch/extension/llm/runner/io_manager/io_manager.h>
#include <executorch/extension/llm/runner/llm_runner_helper.h>
#include <executorch/extension/llm/runner/text_decoder_runner.h>
#include <executorch/extension/llm/runner/text_prefiller.h>
#include <executorch/extension/threadpool/threadpool.h>
#include <executorch/extension/tensor/tensor.h>
#include <executorch/runtime/core/error.h>

#include "Logger.hpp"

namespace {
using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double>;
using executorch::aten::ScalarType;
using executorch::extension::from_blob;
using executorch::extension::Module;
using executorch::extension::llm::GenerationConfig;
using executorch::extension::llm::IOManager;
using executorch::extension::llm::TextDecoderRunner;
using executorch::extension::llm::TextPrefiller;
using executorch::extension::llm::get_eos_ids;
using executorch::extension::llm::get_llm_metadata;
using executorch::extension::llm::kEnableDynamicShape;
using executorch::extension::llm::kMaxContextLen;
using executorch::extension::llm::kMaxSeqLen;
using executorch::extension::llm::kUseKVCache;
using executorch::extension::llm::load_tokenizer;
using executorch::runtime::Error;

float TokensPerSecond(size_t tokenCount, double seconds)
{
    if (tokenCount == 0 || seconds <= 0.0) {
        return 0.0f;
    }
    return static_cast<float>(static_cast<double>(tokenCount) / seconds);
}

void ThrowGenerationError(const char* operation, Error error)
{
    if (error == Error::Ok) {
        return;
    }

    if (error == Error::InvalidArgument) {
        THROW_ERROR("ExecuTorch %s failed: invalid argument or context is full", operation);
    }
    THROW_ERROR("ExecuTorch %s failed with error=%d", operation, static_cast<int>(error));
}

} // namespace

/**
 * Thin synchronous wrapper around ExecuTorch's lower-level text runner pieces.
 * The public LLM API still accepts text today, but this class keeps text
 * tokenization, token prefill, one-step decode, and detokenization as separate
 * stages so token-based entry points can reuse the same runtime path later.
 */
class LLM::LLMImpl::SynchronousTextRunner {
public:
    /**
     * Build the runner and wire the tokenizer, module, IO manager, decoder, and prefiller.
     *
     * @param modelPath Path to the ExecuTorch model file.
     * @param tokenizer Loaded tokenizer owned by the runner after creation.
     * @return A configured runner, or nullptr if tokenizer or model metadata setup fails.
     */
    static std::unique_ptr<SynchronousTextRunner> Create(
        const std::string& modelPath,
        std::unique_ptr<tokenizers::Tokenizer> tokenizer)
    {
        if (!tokenizer || !tokenizer->is_loaded()) {
            LOG_ERROR("ExecuTorch tokenizer is null or not loaded");
            return nullptr;
        }

        auto module = std::make_unique<Module>(modelPath, Module::LoadMode::MmapUseMlockIgnoreErrors);
        auto metadataResult = get_llm_metadata(tokenizer.get(), module.get());
        if (metadataResult.error() != Error::Ok) {
            LOG_ERROR("ExecuTorch failed to read model metadata");
            return nullptr;
        }
        auto metadata = metadataResult.get();
        auto eosIds = get_eos_ids(tokenizer.get(), module.get());

        auto ioManager = std::make_unique<IOManager>(*module);
        auto decoder = std::make_unique<TextDecoderRunner>(module.get(), ioManager.get(), "forward");
        auto prefiller = std::make_unique<TextPrefiller>(
            decoder.get(),
            metadata.at(kUseKVCache),
            metadata.at(kEnableDynamicShape),
            metadata.at(kMaxSeqLen));

        return std::unique_ptr<SynchronousTextRunner>(new SynchronousTextRunner(
            std::move(metadata),
            std::move(eosIds),
            std::move(tokenizer),
            std::move(module),
            std::move(ioManager),
            std::move(decoder),
            std::move(prefiller)));
    }

    /**
     * Load ExecuTorch runtime state once before prefill or decode.
     *
     * @return Error::Ok on success, or the ExecuTorch load error.
     */
    Error Load()
    {
        if (m_loaded) {
            return Error::Ok;
        }

        Error error = m_prefiller->load();
        if (error != Error::Ok) {
            return error;
        }
        error = m_ioManager->load();
        if (error != Error::Ok) {
            return error;
        }

        m_loaded = true;
        return Error::Ok;
    }

    /**
     * Clear conversation state, including the KV-cache position.
     */
    void Reset()
    {
        // Full conversation reset: clear both the active generation and the
        // accumulated KV-cache position used across chat turns.
        ResetGeneration();
        m_pos = 0;
        m_numPromptTokens = 0;
        m_currentToken = 0;
        m_decodeTokens.clear();
        m_decodeTokenData[0] = 0;
    }

    /**
     * Clear only the active response stream while preserving conversation context.
     */
    void ResetGeneration()
    {
        // Stop only the current response stream. The KV cache stays intact so
        // the next user turn can continue the conversation unless Reset() runs.
        m_active = false;
        m_stopped = false;
        m_hasPendingPrefillToken = false;
        m_generatedTokens = 0;
        m_maxNewTokens = 0;
    }

    /**
     * Stop the active generation and notify the ExecuTorch decoder.
     */
    void Stop()
    {
        m_stopped = true;
        m_active = false;
        m_hasPendingPrefillToken = false;
        if (m_decoder) {
            m_decoder->stop();
        }
    }

    /**
     * Convert a text prompt to model token IDs with the requested BOS/EOS policy.
     *
     * @param prompt Text prompt to tokenize.
     * @param config Generation settings that control BOS/EOS token insertion.
     * @param tokens Output token IDs.
     * @return Error::Ok on success, or Error::InvalidArgument if tokenization fails.
     */
    Error Tokenize(const std::string& prompt, const GenerationConfig& config, std::vector<uint64_t>& tokens)
    {
        // Keep tokenization separate from prefill so callers can eventually
        // bypass text input and provide model-ready token IDs directly.
        auto encodeResult = m_tokenizer->encode(prompt, config.num_bos, config.num_eos);
        if (!encodeResult.ok()) {
            return Error::InvalidArgument;
        }

        tokens = encodeResult.get();
        return tokens.empty() ? Error::InvalidArgument : Error::Ok;
    }

    /**
     * Convert a generated token into the text piece that should be emitted.
     *
     * @param previousToken Previous token ID, used by tokenizers that need context.
     * @param token Current generated token ID.
     * @param piece Output decoded text piece.
     * @return Error::Ok on success, or Error::InvalidArgument if decoding fails.
     */
    Error Detokenize(uint64_t previousToken, uint64_t token, std::string& piece)
    {
        // Tokenizers may need the previous token to reconstruct whitespace or
        // byte-pair boundaries for the emitted text piece.
        auto decodeResult = m_tokenizer->decode(previousToken, token);
        if (!decodeResult.ok()) {
            return Error::InvalidArgument;
        }

        piece = decodeResult.get();
        return Error::Ok;
    }

    /**
     * Tokenize and prefill a text prompt, returning the number of prompt tokens consumed.
     *
     * @param prompt Text prompt to tokenize and prefill.
     * @param config Generation settings for tokenization and prefill.
     * @param promptTokens Output number of prompt tokens consumed.
     * @return Error::Ok on success, or the tokenization/prefill error.
     */
    Error Prefill(const std::string& prompt, const GenerationConfig& config, size_t& promptTokens)
    {
        std::vector<uint64_t> tokens;
        const Error tokenizeError = Tokenize(prompt, config, tokens);
        if (tokenizeError != Error::Ok) {
            return tokenizeError;
        }

        return PrefillTokens(std::move(tokens), config, promptTokens);
    }

    /**
     * Prefill already-tokenized input and prepare the first generated token.
     *
     * @param tokens Prompt token IDs to prefill.
     * @param config Generation settings for prefill and subsequent decode.
     * @param promptTokens Output number of prompt tokens consumed.
     * @return Error::Ok on success, or the ExecuTorch prefill/load error.
     */
    Error PrefillTokens(std::vector<uint64_t> tokens, const GenerationConfig& config, size_t& promptTokens)
    {
        // This is the token-level entry point for prefill. Text prompts use
        // Tokenize() first, while future APIs can pass token IDs directly.
        Error error = Load();
        if (error != Error::Ok) {
            return error;
        }

        ResetGeneration();
        m_temperature = config.temperature;
        m_ignoreEos = config.ignore_eos;

        if (tokens.empty()) {
            return Error::InvalidArgument;
        }

        const int64_t maxContextLen = metadataValue(kMaxContextLen) - m_pos;
        if (static_cast<int64_t>(tokens.size()) >= maxContextLen) {
            return Error::InvalidArgument;
        }

        // TextPrefiller advances m_pos to the next KV-cache position and
        // returns the first sampled token, which belongs to the decode stream.
        auto prefillResult = m_prefiller->prefill(tokens, m_pos);
        if (prefillResult.error() != Error::Ok) {
            return prefillResult.error();
        }

        m_currentToken = prefillResult.get();
        m_decodeTokenData[0] = m_currentToken;
        // Keep the active token window for no-KV-cache models. KV-cache models
        // only consume m_decodeTokenData during decode, but retaining the full
        // vector keeps both paths backed by the same prompt token ownership.
        m_decodeTokens = std::move(tokens);
        m_decodeTokens.push_back(m_currentToken);
        m_numPromptTokens = static_cast<int64_t>(m_decodeTokens.size() - 1);
        promptTokens = static_cast<size_t>(m_numPromptTokens);

        m_maxNewTokens = config.resolve_max_new_tokens(
            static_cast<int32_t>(maxContextLen),
            static_cast<int32_t>(m_numPromptTokens));
        if (m_maxNewTokens <= 0) {
            return Error::InvalidArgument;
        }

        m_generatedTokens = 0;
        m_hasPendingPrefillToken = true;
        m_active = true;
        m_stopped = false;
        return Error::Ok;
    }

    /**
     * Emit the next decoded text piece, or mark the response complete.
     *
     * @param token Output decoded text piece, empty when only completion is reported.
     * @param finished Output flag set when the current response stream is complete.
     * @return Error::Ok on success or normal completion, or the decode/detokenize error.
     */
    Error NextToken(std::string& token, bool& finished)
    {
        token.clear();
        finished = false;

        // Returning Ok with finished=true lets the outer wrapper emit its
        // framework-agnostic EOS marker without treating completion as failure.
        if (!m_active || m_stopped || m_generatedTokens >= m_maxNewTokens) {
            finished = true;
            return Error::Ok;
        }

        if (m_hasPendingPrefillToken) {
            m_hasPendingPrefillToken = false;
            // Prefill already sampled the first generated token. Emit it here
            // so the benchmark's first NextToken() measures TTFT cleanly.
            const Error detokenizeError = Detokenize(m_currentToken, m_currentToken, token);
            if (detokenizeError != Error::Ok) {
                return detokenizeError;
            }
            ++m_generatedTokens;
            FinishIfNeeded(finished);
            return Error::Ok;
        }

        auto input = MakeDecodeInput();
        auto logitsResult = m_decoder->step(input, m_pos);
        if (logitsResult.error() != Error::Ok) {
            return logitsResult.error();
        }

        const uint64_t previousToken = m_currentToken;
        m_currentToken = static_cast<uint64_t>(m_decoder->logits_to_token(logitsResult.get(), m_temperature));
        ++m_pos;

        if (!UseKvCache()) {
            // Without KV cache, every decode step reruns the model over the
            // full sequence accumulated so far.
            m_decodeTokens.push_back(m_currentToken);
        }
        m_decodeTokenData[0] = m_currentToken;

        const Error detokenizeError = Detokenize(previousToken, m_currentToken, token);
        if (detokenizeError != Error::Ok) {
            return detokenizeError;
        }

        ++m_generatedTokens;
        FinishIfNeeded(finished);
        return Error::Ok;
    }

    /**
     * Return approximate percentage of configured context already consumed.
     *
     * @param contextSize Configured context size.
     * @return Integer percentage of context consumed, or 0 when contextSize is invalid.
     */
    size_t ContextProgress(int contextSize) const
    {
        if (contextSize <= 0) {
            return 0;
        }
        return static_cast<size_t>(100 * (m_pos + m_generatedTokens) / contextSize);
    }

private:
    SynchronousTextRunner(
        std::unordered_map<std::string, int64_t> metadata,
        std::unordered_set<uint64_t> eosIds,
        std::unique_ptr<tokenizers::Tokenizer> tokenizer,
        std::unique_ptr<Module> module,
        std::unique_ptr<IOManager> ioManager,
        std::unique_ptr<TextDecoderRunner> decoder,
        std::unique_ptr<TextPrefiller> prefiller)
        : m_metadata(std::move(metadata))
        , m_eosIds(std::move(eosIds))
        , m_tokenizer(std::move(tokenizer))
        , m_module(std::move(module))
        , m_ioManager(std::move(ioManager))
        , m_decoder(std::move(decoder))
        , m_prefiller(std::move(prefiller))
    {}

    bool UseKvCache() const
    {
        return metadataValue(kUseKVCache) != 0;
    }

    int64_t metadataValue(const std::string& key) const
    {
        const auto it = m_metadata.find(key);
        return it == m_metadata.end() ? 0 : it->second;
    }

    executorch::extension::TensorPtr MakeDecodeInput()
    {
        if (UseKvCache()) {
            // KV-cache decode feeds only the previous generated token plus the
            // current cache position.
            m_decodeTokenData[0] = m_currentToken;
            return from_blob(m_decodeTokenData.data(), {1, 1}, ScalarType::Long);
        }
        // Non-KV-cache decode feeds the complete token history each step.
        return from_blob(
            m_decodeTokens.data(),
            {1, static_cast<int>(m_decodeTokens.size())},
            ScalarType::Long);
    }

    void FinishIfNeeded(bool& finished)
    {
        // EOS and max-token completion both end this response stream, but they
        // do not reset m_pos; the chat context remains available for next turn.
        if (m_generatedTokens >= m_maxNewTokens ||
            (!m_ignoreEos && m_eosIds.find(m_currentToken) != m_eosIds.end())) {
            m_active = false;
            finished = true;
        }
    }

    std::unordered_map<std::string, int64_t> m_metadata{};
    std::unordered_set<uint64_t> m_eosIds{};
    std::unique_ptr<tokenizers::Tokenizer> m_tokenizer{nullptr};
    std::unique_ptr<Module> m_module{nullptr};
    std::unique_ptr<IOManager> m_ioManager{nullptr};
    std::unique_ptr<TextDecoderRunner> m_decoder{nullptr};
    std::unique_ptr<TextPrefiller> m_prefiller{nullptr};
    std::vector<uint64_t> m_decodeTokens{};
    std::vector<uint64_t> m_decodeTokenData{0};
    uint64_t m_currentToken{0};
    // KV-cache write/read position. TextPrefiller advances this over prompt
    // tokens; each synchronous decode step advances it over generated tokens.
    int64_t m_pos{0};
    int64_t m_numPromptTokens{0};
    int64_t m_generatedTokens{0};
    int32_t m_maxNewTokens{0};
    float m_temperature{0.0f};
    bool m_ignoreEos{false};
    bool m_loaded{false};
    bool m_active{false};
    bool m_stopped{false};
    bool m_hasPendingPrefillToken{false};
};

LLM::LLMImpl::LLMImpl() = default;

LLM::LLMImpl::~LLMImpl()
{
    FreeLlm();
}

void LLM::LLMImpl::LlmInit(const LlmConfig& config, std::string sharedLibraryPath)
{
    m_config = config;
    m_modelPath = config.GetConfigString(LlmConfig::ConfigParam::LlmModelName);
    m_tokenizerPath = ResolveTokenizerPath();
    m_sharedLibraryPath = std::move(sharedLibraryPath);
    m_nCtx = config.GetConfigInt(LlmConfig::ConfigParam::ContextSize);
    m_numThreads = config.GetConfigInt(LlmConfig::ConfigParam::NumThreads);
    m_contextFilled = 0;
    ConfigureThreadPool();

    auto tokenizer = load_tokenizer(m_tokenizerPath);
    if (!tokenizer) {
        THROW_ERROR("ExecuTorch tokenizer initialization failed for tokenizer='%s'", m_tokenizerPath.c_str());
    }
    WarnIfContextSizeExceedsModelLimit(tokenizer.get());

    m_runner = SynchronousTextRunner::Create(m_modelPath, std::move(tokenizer));
    if (!m_runner) {
        THROW_ERROR("ExecuTorch runner creation failed for model='%s'", m_modelPath.c_str());
    }

    const Error loadError = m_runner->Load();
    if (loadError != Error::Ok) {
        THROW_ERROR("ExecuTorch model load failed for model='%s' error=%d",
                    m_modelPath.c_str(), static_cast<int>(loadError));
    }

    m_initialized = true;

    LOG_INF("ExecuTorch initialized with model='%s' tokenizer='%s' threads=%d",
            m_modelPath.c_str(), m_tokenizerPath.c_str(), m_numThreads);
}

void LLM::LLMImpl::FreeLlm()
{
    if (!m_initialized && !m_runner) {
        return;
    }

    StopGeneration();
    m_runner.reset();
    m_promptTokenizer.reset();
    m_initialized = false;
    ResetTimings();
    LOG_INF("Freed ExecuTorch LLM");
}

float LLM::LLMImpl::GetEncodeTimings()
{
    return TokensPerSecond(m_totalEncodedTokens, m_totalEncoderTime);
}

float LLM::LLMImpl::GetDecodeTimings()
{
    return TokensPerSecond(m_totalDecodedTokens, m_totalDecoderTime);
}

void LLM::LLMImpl::ResetTimings()
{
    m_totalDecodedTokens = 0;
    m_totalEncodedTokens = 0;
    m_totalDecoderTime = 0.0;
    m_totalEncoderTime = 0.0;
}

std::string LLM::LLMImpl::SystemInfo()
{
    EnsureInitialized("SystemInfo");
    return "System INFO:\nFramework: ExecuTorch\nModel: " + m_modelPath + "\n";
}

void LLM::LLMImpl::ResetContext()
{
    EnsureInitialized("ResetContext");

    StopGeneration();
    m_runner->Reset();

    m_isConversationStart = true;
    m_contextFilled = 0;

    ResetTimings();
    LOG_INF("Reset ExecuTorch context");
}

void LLM::LLMImpl::Encode(LlmChat::Payload& payload)
{
    EnsureInitialized("Encode");

    StopGeneration();

    GenerationConfig generationConfig;
    generationConfig.echo = false;
    generationConfig.temperature = 0.0f;
    generationConfig.seq_len = m_nCtx;
    generationConfig.max_new_tokens = -1;

    size_t promptTokens = 0;
    std::vector<uint64_t> promptTokenIds;
    const auto tStart = Clock::now();
    const Error tokenizeError = m_runner->Tokenize(payload.textPrompt, generationConfig, promptTokenIds);
    ThrowGenerationError("tokenize", tokenizeError);
    const Error prefillError = m_runner->PrefillTokens(std::move(promptTokenIds), generationConfig, promptTokens);
    const auto tEnd = Clock::now();
    ThrowGenerationError("prefill", prefillError);

    m_totalEncodedTokens += promptTokens;
    m_totalEncoderTime += Duration(tEnd - tStart).count();
    m_contextFilled = m_runner->ContextProgress(m_nCtx);
}

std::string LLM::LLMImpl::NextToken()
{
    EnsureInitialized("NextToken");

    std::string token;
    bool finished = false;
    const auto tStart = Clock::now();
    const Error decodeError = m_runner->NextToken(token, finished);
    const auto tEnd = Clock::now();
    ThrowGenerationError("decode", decodeError);

    if (finished && token.empty()) {
        return m_eos;
    }

    if (!token.empty()) {
        ++m_totalDecodedTokens;
        m_totalDecoderTime += Duration(tEnd - tStart).count();
    }
    m_contextFilled = m_runner->ContextProgress(m_nCtx);
    return token;
}

void LLM::LLMImpl::Cancel()
{
    LOG_INF("Cancelling ExecuTorch generation");
    StopGeneration();
}

size_t LLM::LLMImpl::GetChatProgress() const
{
    return m_contextFilled;
}

void LLM::LLMImpl::StopGeneration()
{
    if (m_runner) {
        m_runner->Stop();
    }
}

bool LLM::LLMImpl::ApplyAutoChatTemplate(LlmChat::Payload& payload)
{
    (void)payload;
    return false;
}

std::string LLM::LLMImpl::GeneratePromptWithNumTokens(size_t numPromptTokens)
{
    EnsureInitialized("GeneratePromptWithNumTokens");
    if (numPromptTokens == 0) {
        return std::string{};
    }

    const std::string pattern = " A";
    std::string prompt;
    if (!m_promptTokenizer) {
        m_promptTokenizer = load_tokenizer(m_tokenizerPath);
        if (!m_promptTokenizer) {
            THROW_ERROR("ExecuTorch benchmark tokenizer initialization failed for tokenizer='%s'", m_tokenizerPath.c_str());
        }
    }

    while (true) {
        prompt += pattern;
        const auto encodeResult = m_promptTokenizer->encode(prompt);
        if (!encodeResult.ok()) {
            THROW_ERROR("ExecuTorch benchmark prompt tokenization failed");
        }

        const size_t currentTokens = encodeResult.get().size();
        if (currentTokens >= numPromptTokens) {
            return prompt;
        }
    }
}

void LLM::LLMImpl::EnsureInitialized(const char* operation) const
{
    if (!m_initialized) {
        THROW_ERROR("ExecuTorch %s failed: backend not initialized", operation);
    }
}

void LLM::LLMImpl::ConfigureThreadPool()
{
    auto* threadPool = executorch::extension::threadpool::get_threadpool();
    if (!threadPool) {
        THROW_ERROR("ExecuTorch failed to acquire runtime threadpool");
    }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    const bool resetOk = threadPool->_unsafe_reset_threadpool(static_cast<uint32_t>(m_numThreads));
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
    if (!resetOk) {
        THROW_ERROR("ExecuTorch failed to configure CPU thread count: %d", m_numThreads);
    }

    LOG_INF("Configured ExecuTorch runtime threadpool with %zu CPU threads",
            threadPool->get_thread_count());
}

void LLM::LLMImpl::WarnIfContextSizeExceedsModelLimit(tokenizers::Tokenizer* tokenizer) const
{
    Module metadataModule(m_modelPath, Module::LoadMode::MmapUseMlockIgnoreErrors);
    const auto metadataResult = get_llm_metadata(tokenizer, &metadataModule);
    if (metadataResult.error() != Error::Ok) {
        return;
    }

    const auto metadata = metadataResult.get();
    const auto maxContextIt = metadata.find(kMaxContextLen);
    if (maxContextIt == metadata.end()) {
        return;
    }

    const int64_t modelMaxContextLength = maxContextIt->second;
    if (m_nCtx > modelMaxContextLength) {
        LOG_WARN("ExecuTorch config contextSize=%d exceeds exported model max context length=%" PRId64
                 ". ExecuTorch cannot increase context length at runtime; re-export the model with a larger max context length.",
                 m_nCtx,
                 modelMaxContextLength);
    }
}

std::string LLM::LLMImpl::ResolveTokenizerPath() const
{
    const std::filesystem::path modelPath(m_modelPath);
    const std::filesystem::path modelDir = modelPath.parent_path();
    const std::vector<std::string> tokenizerNames = {
        "tokenizer.model",
        "tokenizer.json",
        "tokenizer.bin",
    };

    for (const auto& tokenizerName : tokenizerNames) {
        const std::filesystem::path tokenizerPath = modelDir / tokenizerName;
        if (std::filesystem::exists(tokenizerPath)) {
            return tokenizerPath.string();
        }
    }

    THROW_ERROR("ExecuTorch initialization failed: tokenizer file not found in '%s'. Expected tokenizer.model, tokenizer.json, or tokenizer.bin",
                modelDir.string().c_str());
}
