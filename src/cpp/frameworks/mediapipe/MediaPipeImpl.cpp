//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmImpl.hpp"
#include <string>
#include <mutex>
#include <iostream>
#include <filesystem>


#define LOG_INF(...)                  \
    do {                              \
        fprintf(stdout, __VA_ARGS__); \
    } while (0)

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
        std::cerr << "Error getting current directory: " << e.what() << std::endl;
        return "";
    }
}

std::string GetExtraStringPart(const std::string& resultString, const std::string& llmCallbackString) {

    if (llmCallbackString.size() >= resultString.size() && llmCallbackString.compare(0, resultString.size(), resultString) == 0) {
        return llmCallbackString.substr(resultString.size());
    }
    return ""; // resultString we extracted is not a prefix of llmEngine's response string
}

void LlmCallback(void* ctx, LlmResponseContext* response_context) {
    // Register as cpu_callback function inside Llm's Inference Engine
    auto* context = reinterpret_cast<CallbackContext*>(ctx);
    std::string token(response_context->response_array[0]);
    context->m_asyncResponse += std::string(response_context->response_array[0]);
    context->m_nCur++;
    if (response_context->done) {
        context->m_done = true;
    }
    else {
        context->m_done = false;
    }
    // Notify the NextToken function to extract predicted token
    context->m_callbackStatus.notify_one();
}

void LLM::LLMImpl::LoadEngine(const std::string& model_path, const std::string& cache_dir)
{
    const LlmModelSettings model_settings = {
        .model_path     = model_path.c_str(),
        .cache_dir      = cache_dir.c_str(),
        .max_num_tokens = this->m_nCtx,
    };

    this->m_errorCode =
        LlmInferenceEngine_CreateEngine(&model_settings, &this->m_llmEngine, &this->m_errorMsg);
    if (this->m_errorCode) {
        LOG_INF("Failed to create engine: %s", this->m_errorMsg);
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
        LOG_INF("Failed to load session: %s", this->m_errorMsg);
        free(this->m_errorMsg);
    }
}

void LLM::LLMImpl::LlmInit(const LlmConfig& config)
{
    try {
        const std::string modelPath = config.GetModelPath();
        const std::string cache_dir = GetCacheDir();

        std::filesystem::create_directories(cache_dir);
        LoadEngine(modelPath, cache_dir);

        if (this->m_errorCode) {
            throw std::runtime_error("Mediapipe Engine creation failed");
        }

        LoadSession();
        if (this->m_errorCode) {
            throw std::runtime_error("Mediapipe Session creation failed");
        }

        this->m_llmPrefix           = config.GetLlmPrefix();
        this->m_conversationContext = "";
        this->m_llmInitialized      = true;
        this->m_callbackContext.m_nCur = 0;
    } catch (const std::exception& e) {
        throw std::runtime_error("LLM initialization failed: " + std::string(e.what()));
    }
}

void LLM::LLMImpl::Encode(std::string& query)
{
    this->m_callbackContext.m_done = false;
    std::string _query = this->m_conversationContext + query;
    this->m_errorCode  = LlmInferenceEngine_Session_AddQueryChunk(
        this->m_llmEngineSession, _query.c_str(), &this->m_errorMsg);
    if (this->m_errorCode) {
        LOG_INF("Encoding failed: %s\n", this->m_errorMsg);
        free(this->m_errorMsg);
        return;
    }
    this->m_conversationContext = _query;
    this->m_callbackContext.m_nCur =  LlmInferenceEngine_Session_SizeInTokens(this->m_llmEngineSession, _query.c_str(), &this->m_errorMsg);
    if (this->m_callbackContext.m_nCur < 0) {
        free(this->m_errorMsg);
        throw std::runtime_error("Mediapipe Chat Progress Finder failed: \n");
        return;
    }
    // clear response strings for synchronization
    this->m_callbackContext.m_asyncResponse.clear();
    this->m_singleResponse.clear();

    // The prediction can start as soon we encode. extract the tokens twith NextToken call.
    this->m_errorCode = LlmInferenceEngine_Session_PredictAsync(
    this->m_llmEngineSession,&this->m_callbackContext, &this->m_errorMsg, LlmCallback);
    if (this->m_errorCode) {
        LOG_INF("Retrieval of next token failed: %s\n", this->m_errorMsg);
        free(this->m_errorMsg);
    }
}

std::string LLM::LLMImpl::NextToken()
{
    std::string response;
    if (this->m_callbackContext.m_done)
        return "";
    bool complete = false;
     {
        std::unique_lock<std::mutex> lock(this->m_callbackContext.m_callbackMutex);

        // Wait unconditionally for a notification
        this->m_callbackContext.m_callbackStatus.wait(lock);
        //response the part of singleResponse-string not yet extracted from llmEngine.
        response = GetExtraStringPart(this->m_singleResponse, this->m_callbackContext.m_asyncResponse);
        // the lock helps to synchronize the completion of response
        complete = this->m_callbackContext.m_done;
    };
    this->m_conversationContext += response;
    this->m_singleResponse += response;
    return  complete? response + m_eos : response;
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
        LlmInferenceEngine_Session_SizeInTokens(this->m_llmEngineSession, text, &this->m_errorMsg);
    if (nPrefix < 0) {
        free(this->m_errorMsg);
        throw std::runtime_error("Mediapipe Prefix Length Finder failed: /n");
        return -1;
    }
    return nPrefix;
}

void LLM::LLMImpl::ResetContext()
{
    if (!this->m_llmPrefix.empty()) {
        try {
            auto n_prefix               = GetInitialPromptLength(this->m_llmPrefix.c_str());
            this->m_callbackContext.m_nCur                = n_prefix;
            this->m_conversationContext = this->m_llmPrefix;
        } catch (const std::exception& e) {
            LOG_INF("Context reset failed: %s", e.what());
        }
    } else {
        KVCacheClear();
        this->m_callbackContext.m_nCur  = 0;
        this->m_conversationContext = "";
    }
}

size_t LLM::LLMImpl::GetChatProgress()
{
     return (this->m_callbackContext.m_nCur * 100) / this->m_nCtx;
}

std::string LLM::LLMImpl::GetFrameworkType()
{
    return this->m_frameworkType;
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
}

std::string LLM::LLMImpl::BenchModel(int& prompts, int& eval_prompts, int& n_max_sq, int& n_rep)
{
    // TODO: Refactor BenchModel() into a framework-agnostic utility:
    // Abstract the core benchmarking logic into a shared BenchModel(const Config&) function,
    // Migrate each framework submodule to invoke it, and consolidate all parameters into the Config struct.
    return (char *) nullptr;
}

void LLM::LLMImpl::StopGeneration()
{
     // Signal to cancel the response , helps in sending next query
     LlmInferenceEngine_Session_PendingProcessCancellation(static_cast<LlmInferenceEngine_Session*>(this->m_llmEngineSession),&this->m_errorMsg);
}

