//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmImpl.hpp"
#include <chrono>
#include <nlohmann/json.hpp>

#define LOG_INF(...)                  \
    do {                              \
        fprintf(stdout, __VA_ARGS__); \
    } while (0)

using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = std::chrono::duration<double>;

/**
 * @brief ONNX Implementation of our LLM API
 *
 */
LLM::LLMImpl::LLMImpl() {}

LLM::LLMImpl::~LLMImpl()
{
    this->FreeLlm();
}

void LLM::LLMImpl::InitSequence()
{
    this->m_sequencesPtr = OgaSequences::Create();

    if (this->m_sequencesPtr == nullptr) {
        throw std::runtime_error("Error: unable to init sequence");
    }

    LOG_INF("Seuqence Initialized\n");
}

void LLM::LLMImpl::FreeSequence()
{
    if (this->m_sequencesPtr) {
        this->m_sequencesPtr.reset();
        this->m_sequencesPtr = nullptr;
        LOG_INF("Freed Sequences\n");
    }
}

void LLM::LLMImpl::InitConfigs()
{
    // genai_config.json path (same as model path)
    this->m_llmConfigsPtr = OgaConfig::Create(this->m_modelPath.c_str());

    if (this->m_llmConfigsPtr == nullptr) {
        throw std::runtime_error("Error: configs initialization failed");
    }

    // This will fall back to default provider which is: CPU
    this->m_llmConfigsPtr->ClearProviders();

    // Currently we modify only thread numbers, but different session options can be modified
    // Ref: https://github.com/microsoft/onnxruntime-genai/blob/79d1d8470b74564fc4e723312a476e692057b600/src/config.h#L64
    std::string patch =
        std::string(R"json({
            "model": {
                "decoder": {
                "session_options": {
                    "intra_op_num_threads": )json") + std::to_string(this->m_numOfThreads) +
                R"json(,
                    "inter_op_num_threads": 1
                }
                }
            }
            })json";

    this->m_llmConfigsPtr->Overlay(patch.c_str());
    LOG_INF("Configs Initialized\n");
}

void LLM::LLMImpl::FreeConfigs()
{
    if (this->m_llmConfigsPtr) {
        this->m_llmConfigsPtr.reset();
        this->m_llmConfigsPtr = nullptr;
        LOG_INF("Freed Configs\n");
    }
}

void LLM::LLMImpl::InitGenerator()
{
    this->m_llmGntParamsPtr = OgaGeneratorParams::Create(* this->m_llmModelPtr);

    if (this->m_llmGntParamsPtr == nullptr) {
        throw std::runtime_error("Error: generator params initialization failed");
    }

    this->m_llmGntParamsPtr->SetSearchOption("max_length", this->m_nCtx);
    this->m_llmGntParamsPtr->SetSearchOption("temperature", 0.0);
    this->m_llmGntParamsPtr->SetSearchOption("top_k", 0.0);
    this->m_llmGntParamsPtr->SetSearchOption("top_p", 1.0);
    this->m_llmGntParamsPtr->SetSearchOptionBool("do_sample", false);
    this->m_llmGntParamsPtr->SetSearchOption("batch_size", this->m_batchSz);

    this->m_llmGeneratorPtr = OgaGenerator::Create(* this->m_llmModelPtr, * this->m_llmGntParamsPtr);

    if (this->m_llmGeneratorPtr == nullptr) {
        throw std::runtime_error("Error: generator initialization failed. Unable to create ONNX generator");
    }

    LOG_INF("Generator Initialized\n");
}

void LLM::LLMImpl::FreeGenerator()
{
    if (this->m_llmGeneratorPtr) {
        this->m_llmGntParamsPtr.reset();
        this->m_llmGntParamsPtr = nullptr;

        this->m_llmGeneratorPtr.reset();
        this->m_llmGeneratorPtr = nullptr;
        LOG_INF("Freed Generator\n");
    }
}

void LLM::LLMImpl::InitTokenizer()
{
    this->m_tokenizerPtr = OgaTokenizer::Create(*this->m_llmModelPtr);

    if (this->m_tokenizerPtr == nullptr) {
        throw std::runtime_error("Error: tokenizer initialization failed");
    }

    this->m_tokenizerStreamPtr = OgaTokenizerStream::Create(*this->m_tokenizerPtr);

    if (this->m_tokenizerStreamPtr == nullptr) {
        throw std::runtime_error("Error: tokenizer stream initialization failed");
    }

    LOG_INF("Tokenizer Initialized\n");
    LOG_INF("Tokenizer Stream Initialized\n");
}

void LLM::LLMImpl::FreeTokenizer()
{
    if (this->m_tokenizerPtr) {
        this->m_tokenizerPtr.reset();
        this->m_tokenizerPtr = nullptr;

        this->m_tokenizerStreamPtr.reset();
        this->m_tokenizerStreamPtr = nullptr;
        LOG_INF("Freed Tokenizer\n");
    }
}

void LLM::LLMImpl::LoadModel()
{
    this->m_llmModelPtr = OgaModel::Create(* this->m_llmConfigsPtr);

    if (this->m_llmModelPtr == nullptr) {
        throw std::runtime_error("Error: unable to load model from " + std::string(this->m_modelPath));
    }

    LOG_INF("Model Loaded\n");
}

void LLM::LLMImpl::FreeModel()
{
    if (this->m_llmModelPtr) {
        this->m_llmModelPtr.reset();
        this->m_llmModelPtr = nullptr;
        LOG_INF("Freed Model\n");
    }
    this->ResetContext();
    this->m_llmInitialized = false;
}

void LLM::LLMImpl::LlmInit(const LlmConfig& config)
{
    try {
        this->m_config            = config;
        this->m_numOfThreads      = config.GetNumThreads();
        this->m_modelPath         = config.GetModelPath().c_str();
        this->m_systemPrompt      = config.GetSystemPrompt();
        this->m_isDefaultTemplate = config.IsDefaultTemplate();
        this->m_systemTemplate    = config.GetSystemTemplate();
        this->m_userTemplate      = config.GetUserTemplate();
        this->m_batchSz           = config.GetBatchSize();

        InitConfigs();

        if (this->m_llmConfigsPtr != nullptr) {
            LoadModel();
        }
        else {
            LOG_INF("Config is not initialized\n");
        }

        if (this->m_llmModelPtr != nullptr) {
            InitTokenizer();
            InitGenerator();
        }

        else {
            LOG_INF("Model is not loaded\n");
        }

        if (this->m_llmConfigsPtr      != nullptr &&
            this->m_tokenizerStreamPtr != nullptr &&
            this->m_llmGeneratorPtr    != nullptr) {

            this->m_llmInitialized = true;
        }

        else {
            this->m_llmInitialized = false;
        }

    } catch (const std::exception& e) {
        throw std::runtime_error("LLM initialization failed: " + std::string(e.what()));
    }

    LOG_INF("LLM Initialized\n");
}

void LLM::LLMImpl::FreeLlm()
{
    if (this->m_llmInitialized) {
        ResetContext();
        FreeConfigs();
        FreeModel();
        FreeGenerator();
        FreeTokenizer();
        FreeSequence();
        ResetTimings();
        this->m_llmInitialized = false;
        LOG_INF("Freed Entire LLM\n");
    }
}

void LLM::LLMImpl::ResetContext()
{
    this->m_llmGeneratorPtr->RewindTo(0);
    this->m_isConversationStart = true;
    ResetTimings();
    this->m_contextFilled = 0;
    LOG_INF("Reset Context\n");
}

std::string LLM::LLMImpl::QueryBuilder(EncodePayload& payload)
{
    if(this->m_isDefaultTemplate) {
        return ApplyDefaultChatTemplate(payload.textPrompt);
    }
    return ApplyAutoChatTemplate(payload.textPrompt);
}

std::string LLM::LLMImpl::ApplyDefaultChatTemplate(const std::string& prompt)
{
    constexpr const char* kPlaceholder = "%s";
    constexpr size_t kPlaceholderSize = std::char_traits<char>::length(kPlaceholder);

    std::string userTurn = this->m_userTemplate;
    if (auto pos = userTurn.find(kPlaceholder); pos != std::string::npos) {
        userTurn.replace(pos, kPlaceholderSize, std::string(prompt));
    } else {
        throw std::runtime_error(
            "Placeholder not found in user template. Please include " + std::string(kPlaceholder) +
            " in the 'userTemplate' section of the configuration file.");}

    if (!this->m_isConversationStart) {
        return userTurn;
    }

    this->m_isConversationStart = false;
    std::string systemTurn = this->m_systemTemplate;

    if (auto pos = systemTurn.find(kPlaceholder); pos != std::string::npos) {
       systemTurn.replace(pos, kPlaceholderSize, std::string(this->m_systemPrompt));
    } else {
        throw std::runtime_error(
            "Placeholder not found in system template. Please include " + std::string(kPlaceholder) +
            " in the 'systemTemplate' section of the configuration file.");}

    return systemTurn + userTurn;
}

std::string LLM::LLMImpl::ApplyAutoChatTemplate(const std::string& prompt)
{
    nlohmann::json messages = nlohmann::json::array();
    std::string out{""};
    if (this->m_isConversationStart) {
        messages.push_back({
            {"role", "system"},
            {"content", this->m_systemPrompt}
        });
    }
    messages.push_back({{"role", "user"},{"content", prompt}});
    const std::string messages_json = messages.dump();
    std::string first_err;
    try {
        // Auto-pick template from config if present
        out = std::string(m_tokenizerPtr->ApplyChatTemplate("", messages_json.c_str(), "", true));
    } catch (const std::exception& e) {
        // Fallback to default implementation if auto failed or produced empty output
        LOG_INF("ApplyChatTemplate failed. Falling back to default template");
        out = ApplyDefaultChatTemplate(prompt);
    }
    this->m_isConversationStart = false;
    return out.c_str();
}

void LLM::LLMImpl::Encode(EncodePayload& payload)
{
    std::string prompt = payload.textPrompt;
    // Time start
    TimePoint startTimeStampEncoder = Clock::now();

    InitSequence();

    this->m_tokenizerPtr->Encode(prompt.c_str(), * this->m_sequencesPtr);
    this->m_llmGeneratorPtr->AppendTokenSequences(* this->m_sequencesPtr);

    // Record finishing time
    this->m_totalEncoderTime += Duration(Clock::now() - startTimeStampEncoder).count();
    this->m_totalEncodedTokens += this->m_sequencesPtr->SequenceCount(0);

}

std::string LLM::LLMImpl::NextToken()
{
    if(!this->m_llmGeneratorPtr->IsDone()) {
        // Record starting time
        TimePoint startTimeStampDecoder = Clock::now();

        this->m_llmGeneratorPtr->GenerateNextToken();
        size_t cnt = this->m_llmGeneratorPtr->GetSequenceCount(0);
        int32_t tok = this->m_llmGeneratorPtr->GetSequenceData(0)[cnt - 1];
        auto out = this->m_tokenizerStreamPtr->Decode(tok);

        // Record finishing time
        this->m_totalDecoderTime += Duration(Clock::now() - startTimeStampDecoder).count();
        this->m_totalDecodedTokens += 1;

        size_t nCurr = this->m_llmGeneratorPtr->GetSequenceCount(0);

        this->m_contextFilled = 100 * nCurr / this->m_nCtx;

        return out;
    }

    else {
        return this->m_eos;
    }
}

size_t LLM::LLMImpl::GetChatProgress() const
{
    return this->m_contextFilled;
}

float LLM::LLMImpl::GetEncodeTimings()
{
    auto encoderTPS = this->m_totalEncodedTokens / this->m_totalEncoderTime;
    return encoderTPS;
}

float LLM::LLMImpl::GetDecodeTimings()
{
    auto decoderTPS = this->m_totalDecodedTokens / this->m_totalDecoderTime;
    return decoderTPS;
}

void LLM::LLMImpl::ResetTimings()
{
    this->m_totalDecoderTime = 0;
    this->m_totalEncoderTime = 0;
    this->m_totalDecodedTokens = 0;
    this->m_totalEncodedTokens = 0;
    LOG_INF("Reset Timings\n");

}

std::string LLM::LLMImpl::SystemInfo()
{
    std::string sysInfo = "\nSystem INFO:\n";
    std::string deviceType = std::string(this->m_llmModelPtr->GetDeviceType());
    std::string modelType = std::string(this->m_llmModelPtr->GetType());
    sysInfo += "Device Type: " + deviceType + "\n";
    sysInfo += "Model Type: " +  modelType + "\n";
    return sysInfo;
}

std::string LLM::LLMImpl::BenchModel(int& prompts, int& eval_prompts, int& n_max_sq, int& n_rep)
{
   // TODO: Refactor BenchModel() into a framework-agnostic utility:
   // Abstract the core benchmarking logic into a shared BenchModel(const Config&) function,
   // Migrate each framework submodule to invoke it, and consolidate all parameters into the Config struct.
   return (char *) nullptr;
}

std::string LLM::LLMImpl::GetFrameworkType()
{
    return this->m_frameworkType;
}

void LLM::LLMImpl::StopGeneration()
{
    // TODO: add stop response to support cancelled query
}
