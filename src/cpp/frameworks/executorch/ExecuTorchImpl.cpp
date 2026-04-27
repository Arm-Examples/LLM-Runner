//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmImpl.hpp"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <filesystem>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>

#define LLM_CLOSE _close
#define LLM_DUP _dup
#define LLM_DUP2 _dup2
#define LLM_FILENO _fileno
#define LLM_OPEN _open
#else
#include <fcntl.h>
#include <unistd.h>

#define LLM_CLOSE close
#define LLM_DUP dup
#define LLM_DUP2 dup2
#define LLM_FILENO fileno
#define LLM_OPEN open
#endif

#include <executorch/extension/llm/runner/llm_runner_helper.h>
#include <executorch/extension/llm/runner/text_llm_runner.h>
#include <executorch/extension/threadpool/threadpool.h>
#include <executorch/runtime/core/error.h>

#include "Logger.hpp"

namespace {
using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::duration<double>;
using executorch::extension::Module;
using executorch::extension::llm::GenerationConfig;
using executorch::extension::llm::create_text_llm_runner;
using executorch::extension::llm::get_llm_metadata;
using executorch::extension::llm::kMaxContextLen;
using executorch::extension::llm::load_tokenizer;
using executorch::runtime::Error;

float TokensPerSecond(size_t tokenCount, double seconds)
{
    if (tokenCount == 0 || seconds <= 0.0) {
        return 0.0f;
    }
    return static_cast<float>(static_cast<double>(tokenCount) / seconds);
}

constexpr bool ShouldShowExecuTorchRunnerOutput()
{
    return ACTIVE_LOG_LEVEL >= LOG_LEVEL_DEBUG;
}

#ifdef _WIN32
constexpr const char* kNullOutputPath = "NUL";
constexpr int kWriteOnlyFlag = _O_WRONLY;
#else
constexpr const char* kNullOutputPath = "/dev/null";
constexpr int kWriteOnlyFlag = O_WRONLY;
#endif

// ExecuTorch's LLM runner prints generated tokens and observer reports directly
// to stdout. Keep benchmark output clean unless wrapper debug logging is enabled.
class ScopedStdoutSilencer {
public:
    ScopedStdoutSilencer()
    {
        if (ShouldShowExecuTorchRunnerOutput()) {
            return;
        }

        std::fflush(stdout);
        m_savedStdout = LLM_DUP(LLM_FILENO(stdout));
        m_nullOutput = LLM_OPEN(kNullOutputPath, kWriteOnlyFlag);
        if (m_savedStdout >= 0 && m_nullOutput >= 0) {
            LLM_DUP2(m_nullOutput, LLM_FILENO(stdout));
        }
    }

    ~ScopedStdoutSilencer()
    {
        if (ShouldShowExecuTorchRunnerOutput()) {
            return;
        }

        std::fflush(stdout);
        if (m_savedStdout >= 0) {
            LLM_DUP2(m_savedStdout, LLM_FILENO(stdout));
            LLM_CLOSE(m_savedStdout);
        }
        if (m_nullOutput >= 0) {
            LLM_CLOSE(m_nullOutput);
        }
    }

    ScopedStdoutSilencer(const ScopedStdoutSilencer&) = delete;
    ScopedStdoutSilencer& operator=(const ScopedStdoutSilencer&) = delete;

private:
    int m_savedStdout{-1};
    int m_nullOutput{-1};
};
} // namespace

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
    m_generationEpoch = m_tokenQueue.reset();
    ConfigureThreadPool();

    auto tokenizer = load_tokenizer(m_tokenizerPath);
    if (!tokenizer) {
        THROW_ERROR("ExecuTorch tokenizer initialization failed for tokenizer='%s'", m_tokenizerPath.c_str());
    }
    WarnIfContextSizeExceedsModelLimit(tokenizer.get());

    m_runner = create_text_llm_runner(m_modelPath, std::move(tokenizer));
    if (!m_runner) {
        THROW_ERROR("ExecuTorch runner creation failed for model='%s'", m_modelPath.c_str());
    }

    const Error loadError = m_runner->load();
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
    m_runner->reset();

    m_isConversationStart = true;
    m_contextFilled = 0;
    m_generationEpoch = m_tokenQueue.reset();

    ResetTimings();
    LOG_INF("Reset ExecuTorch context");
}

void LLM::LLMImpl::Encode(LlmChat::Payload& payload)
{
    EnsureInitialized("Encode");

    StopGeneration();

    // Start a fresh token stream for this generation. TokenQueue's epoch
    // rejects stale callbacks after cancellation or reset.
    m_generationEpoch = m_tokenQueue.reset();
    const uint64_t generationEpoch = m_generationEpoch;
    m_generationError.store(static_cast<int>(Error::Ok), std::memory_order_release);

    // ExecuTorch reports decoded tokens through this callback; TokenQueue is
    // the only synchronization point between the callback and NextToken().
    const auto tokenCallback = [this, generationEpoch](const std::string& token) {
        if (token.empty()) {
            return;
        }
        m_tokenQueue.enqueue(generationEpoch, token);
    };

    const auto statsCallback = [this](const Stats& stats) {
        RecordStats(stats);
    };

    GenerationConfig generationConfig;
    generationConfig.echo = false;
    generationConfig.temperature = 0.0f;
    generationConfig.seq_len = m_nCtx;
    generationConfig.max_new_tokens = -1;

    const std::string prompt = payload.textPrompt;
    m_generationThread = std::thread([this, prompt, generationConfig, tokenCallback, statsCallback, generationEpoch]() {
        Error generateError = Error::Ok;
        {
            ScopedStdoutSilencer stdoutSilencer;
            generateError = m_runner->generate(prompt, generationConfig, tokenCallback, statsCallback);
        }

        if (generateError != Error::Ok) {
            m_generationError.store(static_cast<int>(generateError), std::memory_order_release);
            m_tokenQueue.close(generationEpoch);
            return;
        }

        // Mark normal completion for consumers that read until EOS.
        m_tokenQueue.enqueue(generationEpoch, m_eos);
    });

    if (!m_tokenQueue.waitForToken(generationEpoch)) {
        ThrowIfGenerationFailed();
    }
}

std::string LLM::LLMImpl::NextToken()
{
    EnsureInitialized("NextToken");

    const std::string token = m_tokenQueue.dequeue();
    if (!token.empty()) {
        return token;
    }

    ThrowIfGenerationFailed();
    return m_eos;
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
    // Closing the current epoch wakes blocked NextToken() calls and prevents
    // cancelled callbacks from publishing into the next token stream.
    m_generationEpoch = m_tokenQueue.resetAndClose();
    if (m_runner) {
        m_runner->stop();
    }
    JoinGenerationThread();
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

void LLM::LLMImpl::JoinGenerationThread()
{
    if (m_generationThread.joinable()) {
        m_generationThread.join();
    }
}

void LLM::LLMImpl::RecordStats(const Stats& stats)
{
    m_totalEncodedTokens += static_cast<size_t>(std::max<int64_t>(stats.num_prompt_tokens, 0));
    m_totalDecodedTokens += static_cast<size_t>(std::max<int64_t>(stats.num_generated_tokens, 0));

    const long promptEvalMs = stats.prompt_eval_end_ms - stats.inference_start_ms;
    const long decodeMs = stats.inference_end_ms - stats.prompt_eval_end_ms;
    if (promptEvalMs > 0) {
        m_totalEncoderTime += static_cast<double>(promptEvalMs) / stats.SCALING_FACTOR_UNITS_PER_SECOND;
    }
    if (decodeMs > 0) {
        m_totalDecoderTime += static_cast<double>(decodeMs) / stats.SCALING_FACTOR_UNITS_PER_SECOND;
    }

    if (m_nCtx > 0) {
        const int64_t totalTokens = std::max<int64_t>(
            stats.num_prompt_tokens + stats.num_generated_tokens, 0);
        m_contextFilled = static_cast<size_t>(100 * totalTokens / m_nCtx);
    }
}

void LLM::LLMImpl::ThrowIfGenerationFailed() const
{
    const int errorCode = m_generationError.load(std::memory_order_acquire);
    if (errorCode == static_cast<int>(Error::Ok)) {
        return;
    }

    if (errorCode == static_cast<int>(Error::InvalidArgument)) {
        THROW_ERROR("ExecuTorch generation failed: context is full");
    }
    THROW_ERROR("ExecuTorch generation failed with error=%d", errorCode);
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
