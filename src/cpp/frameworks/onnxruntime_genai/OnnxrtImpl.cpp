//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmImpl.hpp"
#include <chrono>
#include "Logger.hpp"
#include <nlohmann/json.hpp>


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
        THROW_ERROR("Error: unable to init sequence");
    }

    LOG_INF("Sequence Initialized");
}

void LLM::LLMImpl::FreeSequence()
{
    if (this->m_sequencesPtr) {
        this->m_sequencesPtr.reset();
        this->m_sequencesPtr = nullptr;
        LOG_INF("Freed Sequences");
    }
}

void LLM::LLMImpl::InitConfigs()
{
    // genai_config.json path (same as model path)
    this->m_llmConfigsPtr = OgaConfig::Create(this->m_modelPath.c_str());

    if (this->m_llmConfigsPtr == nullptr) {
        THROW_ERROR("Error: configs initialization failed");
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
                "intra_op_num_threads": )json")
        + std::to_string(this->m_numOfThreads)
        + R"json(,
                "inter_op_num_threads": 1,
                "log_severity_level": )json"
        + std::to_string(m_onnxLogMap[ACTIVE_LOG_LEVEL])
        + R"json(
              }
            }
          }
        })json";

    this->m_llmConfigsPtr->Overlay(patch.c_str());
    LOG_INF("Configs Initialized");
}

void LLM::LLMImpl::FreeConfigs()
{
    if (this->m_llmConfigsPtr) {
        this->m_llmConfigsPtr.reset();
        this->m_llmConfigsPtr = nullptr;
        LOG_INF("Freed Configs");
    }
}

void LLM::LLMImpl::InitGenerator()
{
    this->m_llmGntParamsPtr = OgaGeneratorParams::Create(* this->m_llmModelPtr);

    if (this->m_llmGntParamsPtr == nullptr) {
        THROW_ERROR("Error: generator params initialization failed");
    }

    this->m_llmGntParamsPtr->SetSearchOption("max_length", this->m_nCtx);
    this->m_llmGntParamsPtr->SetSearchOption("temperature", 0.0);
    this->m_llmGntParamsPtr->SetSearchOption("top_k", 0.0);
    this->m_llmGntParamsPtr->SetSearchOption("top_p", 1.0);
    this->m_llmGntParamsPtr->SetSearchOptionBool("do_sample", false);
    this->m_llmGntParamsPtr->SetSearchOption("batch_size", 1);

    this->m_llmGeneratorPtr = OgaGenerator::Create(* this->m_llmModelPtr, * this->m_llmGntParamsPtr);

    if (this->m_llmGeneratorPtr == nullptr) {
        THROW_ERROR("Error: generator initialization failed. Unable to create ONNX generator");
    }
    LOG_INF("Generator Initialized");
}

void LLM::LLMImpl::FreeGenerator()
{
    if (this->m_llmGeneratorPtr) {
        this->m_llmGntParamsPtr.reset();
        this->m_llmGntParamsPtr = nullptr;

        this->m_llmGeneratorPtr.reset();
        this->m_llmGeneratorPtr = nullptr;
        LOG_INF("Freed Generator");
    }
}

void LLM::LLMImpl::InitTokenizer()
{
    this->m_tokenizerPtr = OgaTokenizer::Create(*this->m_llmModelPtr);

    if (this->m_tokenizerPtr == nullptr) {
        THROW_ERROR("Error: tokenizer initialization failed");
    }

    this->m_tokenizerStreamPtr = OgaTokenizerStream::Create(*this->m_tokenizerPtr);

    if (this->m_tokenizerStreamPtr == nullptr) {
        THROW_ERROR("Error: tokenizer stream initialization failed");
    }

    LOG_INF("Tokenizer Initialized");
    LOG_INF("Tokenizer Stream Initialized");
}

void LLM::LLMImpl::FreeTokenizer()
{
    if (this->m_tokenizerPtr) {
        this->m_tokenizerPtr.reset();
        this->m_tokenizerPtr = nullptr;

        this->m_tokenizerStreamPtr.reset();
        this->m_tokenizerStreamPtr = nullptr;
        LOG_INF("Freed Tokenizer");
    }
}

void LLM::LLMImpl::LoadModel()
{
    this->m_llmModelPtr = OgaModel::Create(* this->m_llmConfigsPtr);

    if (this->m_llmModelPtr == nullptr) {
        THROW_ERROR("Error: unable to load model from %s", this->m_modelPath.c_str());
    }

    LOG_INF("Model Loaded");
}

void LLM::LLMImpl::FreeModel()
{
    if (this->m_llmModelPtr) {
        this->m_llmModelPtr.reset();
        this->m_llmModelPtr = nullptr;
        LOG_INF("Freed Model");
    }
    this->ResetContext();
    this->m_llmInitialized = false;
}

void LLM::LLMImpl::LlmInit(const LlmConfig& config, std::string sharedLibraryPath = "")
{
    try {
        this->m_config            = config;
        this->m_numOfThreads      = config.GetConfigInt(LlmConfig::ConfigParam::NumThreads);
        this->m_modelPath         = config.GetConfigString(LlmConfig::ConfigParam::LlmModelName);
        this->m_batchSz           = config.GetConfigInt(LlmConfig::ConfigParam::BatchSize);
        this->m_nCtx              = config.GetConfigInt(LlmConfig::ConfigParam::ContextSize);

        InitConfigs();

        if (this->m_llmConfigsPtr != nullptr) {
            LoadModel();
        }
        else {
            LOG_WARN("Config is not initialized");
        }

        if (this->m_llmModelPtr != nullptr) {
            InitTokenizer();
            InitGenerator();
        }

        else {
            LOG_INF("Model is not loaded");
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
        THROW_ERROR("LLM initialization failed: %s", e.what());
    }

    LOG_INF("LLM Initialized");
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
        LOG_INF("Freed Entire LLM");
    }
}

void LLM::LLMImpl::ResetContext()
{
    this->m_llmGeneratorPtr->RewindTo(0);
    this->m_isConversationStart = true;
    ResetTimings();
    this->m_contextFilled = 0;
    this->m_nCurr = 0;
    LOG_INF("Reset Context");
}

bool LLM::LLMImpl::ApplyAutoChatTemplate(LlmChat::Payload& payload)
{
    // Helper: default role is "user"
    auto chatMsg = [](std::string_view content, std::string_view role = "user") -> nlohmann::json {
        return nlohmann::json{{"role", role}, {"content", content}};
    };

    nlohmann::json messages = nlohmann::json::array();

    if (m_isConversationStart) {
        messages.push_back(chatMsg(m_systemPrompt, "system"));
    }
    messages.push_back(chatMsg(payload.textPrompt)); // role defaults to "user"

    const std::string messages_json = messages.dump();

    try {
        // Auto-pick template from config if present
        std::string formatted = std::string(
            m_tokenizerPtr->ApplyChatTemplate("", messages_json.c_str(), "", /*add_generation_prompt=*/true));

        if (formatted.empty()) {
            LOG_INF("ApplyChatTemplate produced empty output. Falling back to default template.");
            return false;
        }

        payload.textPrompt = std::move(formatted);
        return true;
    } catch (const std::exception& e) {
        LOG_INF("ApplyChatTemplate failed: %s . Falling back to default template.", e.what());
        return false;
    }
}

void LLM::LLMImpl::Encode(LlmChat::Payload& payload)
{
    std::string prompt = payload.textPrompt;

    try {
        // Time start
        TimePoint startTimeStampEncoder = Clock::now();

        InitSequence();

        this->m_tokenizerPtr->Encode(prompt.c_str(), * this->m_sequencesPtr);
        if (this->m_nCurr + this->m_sequencesPtr->SequenceCount(0) >= this->m_nCtx) {
            THROW_ERROR("LLM encoding failed ,context is full");
        }
        this->m_llmGeneratorPtr->AppendTokenSequences(*this->m_sequencesPtr);
        // Record finishing time
        this->m_totalEncoderTime += Duration(Clock::now() - startTimeStampEncoder).count();
        this->m_totalEncodedTokens += this->m_sequencesPtr->SequenceCount(0);
        this->m_nCurr += this->m_sequencesPtr->SequenceCount(0);
    }
    catch (const std::exception& e) {
        THROW_ERROR("Failed to evaluate prompt :%s",e.what());
    }

}

std::string LLM::LLMImpl::NextToken()
{
    try {
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

            // Update context fill progress
            this->m_nCurr += 1;
            this->m_contextFilled = 100 * this->m_nCurr / this->m_nCtx;

            return out;
        }
        else {
            return this->m_eos;
        }
    }
    catch (const std::exception& e) {
        THROW_ERROR("Failed to decode next token : %s",e.what());
    }
}

void LLM::LLMImpl::Cancel() {
	LOG_INF("Cancelling current operation");
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
    LOG_INF("Reset Timings");

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

std::string LLM::LLMImpl::GeneratePromptWithNumTokens(size_t numPromptTokens) {
    const char* const base_prompt = "A";

    // 1) Encode the base prompt into sequences
    auto base_prompt_sequences = OgaSequences::Create();
    m_tokenizerPtr->Encode(base_prompt, *base_prompt_sequences);

    // 2) Create generator params that force the output length = numPromptTokens
    auto params = OgaGeneratorParams::Create(* this->m_llmModelPtr);
    params->SetSearchOption("max_length", static_cast<double>(numPromptTokens));
    params->SetSearchOption("min_length", static_cast<double>(numPromptTokens));

    // 3) Create a generator and run until done
    auto generator = OgaGenerator::Create(* this->m_llmModelPtr, * params);
    generator->AppendTokenSequences(*base_prompt_sequences);

    while (!generator->IsDone()) {
        generator->GenerateNextToken();
    }

    // 4) Decode the generated tokens back into a string prompt
    const auto output_sequence_length = generator->GetSequenceCount(0);
    const auto* output_sequence_data  = generator->GetSequenceData(0);

    return std::string{
        m_tokenizerPtr->Decode(output_sequence_data, output_sequence_length)
    };
}

void LLM::LLMImpl::StopGeneration()
{
    // TODO: add stop response to support cancelled query
}
