//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmImpl.hpp"

#include <chrono>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "Logger.hpp"
#include <nlohmann/json.hpp>

using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = std::chrono::duration<double>;

namespace
{

    constexpr size_t kSingleImageCount = 1;
    using json = nlohmann::json;

} // namespace

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

    if (this->m_sequencesPtr == nullptr)
    {
        THROW_ERROR("Error: unable to init sequence");
    }

    LOG_INF("Sequence Initialized");
}

void LLM::LLMImpl::FreeSequence()
{
    if (this->m_sequencesPtr)
    {
        this->m_sequencesPtr.reset();
        this->m_sequencesPtr = nullptr;
        LOG_INF("Freed Sequences");
    }
}

void LLM::LLMImpl::InitConfigs()
{
    // genai_config.json path (same as model path)
    this->m_llmConfigsPtr = OgaConfig::Create(this->m_modelPath.c_str());

    if (this->m_llmConfigsPtr == nullptr)
    {
        THROW_ERROR("Error: configs initialization failed");
    }

    // This will fall back to default provider which is: CPU
    this->m_llmConfigsPtr->ClearProviders();

    const json patchJson = {
        {"model", {
            {"decoder", {
                {"session_options", {
                    {"intra_op_num_threads", this->m_numOfThreads},
                    {"inter_op_num_threads", 1},
                    {"log_severity_level", m_onnxLogMap.at(ACTIVE_LOG_LEVEL)}
                }}
            }}
        }}
    };

    const std::string patch = patchJson.dump();

    this->m_llmConfigsPtr->Overlay(patch.c_str());
    LOG_INF("Configs Initialized");
}

void LLM::LLMImpl::FreeConfigs()
{
    if (this->m_llmConfigsPtr)
    {
        this->m_llmConfigsPtr.reset();
        this->m_llmConfigsPtr = nullptr;
        LOG_INF("Freed Configs");
    }
}

void LLM::LLMImpl::InitGenerator()
{
    FreeGenerator();

    this->m_llmGntParamsPtr = OgaGeneratorParams::Create(*this->m_llmModelPtr);

    if (this->m_llmGntParamsPtr == nullptr)
    {
        THROW_ERROR("Error: generator params initialization failed");
    }

    this->m_llmGntParamsPtr->SetSearchOption("max_length", this->m_nCtx);
    this->m_llmGntParamsPtr->SetSearchOption("temperature", 0.0);
    this->m_llmGntParamsPtr->SetSearchOption("top_k", 0.0);
    this->m_llmGntParamsPtr->SetSearchOption("top_p", 1.0);
    this->m_llmGntParamsPtr->SetSearchOptionBool("do_sample", false);
    this->m_llmGntParamsPtr->SetSearchOption("batch_size", 1);

    this->m_llmGeneratorPtr = CreateGenerator();
    LOG_INF("Generator Initialized");
}

std::unique_ptr<OgaGenerator> LLM::LLMImpl::CreateGenerator() const
{
    if (!this->m_llmModelPtr || !this->m_llmGntParamsPtr)
    {
        THROW_ERROR("ONNX Runtime GenAI generator dependencies are not initialized");
    }

    auto generator = OgaGenerator::Create(*this->m_llmModelPtr, *this->m_llmGntParamsPtr);
    if (generator == nullptr)
    {
        THROW_ERROR("Error: generator initialization failed. Unable to create ONNX generator");
    }
    return generator;
}

void LLM::LLMImpl::FreeGenerator()
{
    if (this->m_llmGeneratorPtr || this->m_llmGntParamsPtr)
    {
        this->m_llmGeneratorPtr.reset();
        this->m_llmGeneratorPtr = nullptr;
        this->m_llmGntParamsPtr.reset();
        this->m_llmGntParamsPtr = nullptr;
        LOG_INF("Freed Generator");
    }
}

std::string LLM::LLMImpl::BuildVisionReplayPrompt(const std::vector<VisionTurn>& turns) const
{
    size_t promptSize = 0;
    for (const auto& turn : turns)
    {
        promptSize += turn.formattedPrompt.size() + turn.assistantResponse.size();
    }

    std::string replayPrompt;
    replayPrompt.reserve(promptSize);
    for (const auto& turn : turns)
    {
        replayPrompt += turn.formattedPrompt;
        replayPrompt += turn.assistantResponse;
    }
    return replayPrompt;
}

void LLM::LLMImpl::RebuildVisionGenerator(const std::vector<VisionTurn>& turns)
{
    if (!this->m_multiModalProcessor)
    {
        THROW_ERROR("ONNX Runtime GenAI multimodal processor is not initialized");
    }

    std::vector<const char*> imagePaths;
    for (const auto& turn : turns)
    {
        if (!turn.imagePath.empty())
        {
            imagePaths.push_back(turn.imagePath.c_str());
        }
    }

    const std::string replayPrompt = BuildVisionReplayPrompt(turns);
    std::unique_ptr<OgaImages> images;
    if (!imagePaths.empty())
    {
        images = OgaImages::Load(imagePaths);
        if (images == nullptr)
        {
            THROW_ERROR("ONNX Runtime GenAI failed to load replay images");
        }
    }

    auto inputTensors = this->m_multiModalProcessor->ProcessImages(replayPrompt.c_str(), images.get());
    auto replacementGenerator = CreateGenerator();
    replacementGenerator->SetInputs(*inputTensors);

    const size_t promptTokens = replacementGenerator->GetSequenceCount(0);
    if (promptTokens >= static_cast<size_t>(this->m_nCtx))
    {
        THROW_ERROR("LLM encoding failed, context is full");
    }

    this->m_llmGeneratorPtr.swap(replacementGenerator);
    FreeSequence();
    this->m_nCurr = promptTokens;
    this->m_contextFilled = 100 * this->m_nCurr / this->m_nCtx;
    this->m_totalEncodedTokens += promptTokens;
}

void LLM::LLMImpl::InitMultiModalProcessor()
{
    if (!this->m_llmModelPtr)
    {
        THROW_ERROR("Error: multimodal processor initialization failed, model is not loaded");
    }

    this->m_multiModalProcessor = OgaMultiModalProcessor::Create(*this->m_llmModelPtr);

    if (this->m_multiModalProcessor == nullptr)
    {
        THROW_ERROR("Error: multimodal processor initialization failed");
    }

    LOG_INF("Multimodal Processor Initialized");
}

void LLM::LLMImpl::FreeMultiModalProcessor()
{
    if (this->m_multiModalProcessor)
    {
        this->m_multiModalProcessor.reset();
        this->m_multiModalProcessor = nullptr;
        LOG_INF("Freed Multimodal Processor");
    }
}

void LLM::LLMImpl::InitTokenizer()
{
    this->m_tokenizerPtr = OgaTokenizer::Create(*this->m_llmModelPtr);

    if (this->m_tokenizerPtr == nullptr)
    {
        THROW_ERROR("Error: tokenizer initialization failed");
    }

    this->m_tokenizerStreamPtr = OgaTokenizerStream::Create(*this->m_tokenizerPtr);

    if (this->m_tokenizerStreamPtr == nullptr)
    {
        THROW_ERROR("Error: tokenizer stream initialization failed");
    }

    LOG_INF("Tokenizer Initialized");
    LOG_INF("Tokenizer Stream Initialized");
}

void LLM::LLMImpl::FreeTokenizer()
{
    if (this->m_tokenizerStreamPtr || this->m_tokenizerPtr)
    {
        this->m_tokenizerStreamPtr.reset();
        this->m_tokenizerStreamPtr = nullptr;
        this->m_tokenizerPtr.reset();
        this->m_tokenizerPtr = nullptr;
        LOG_INF("Freed Tokenizer");
    }
}

void LLM::LLMImpl::LoadModel()
{
    this->m_llmModelPtr = OgaModel::Create(*this->m_llmConfigsPtr);

    if (this->m_llmModelPtr == nullptr)
    {
        THROW_ERROR("Error: unable to load model from %s", this->m_modelPath.c_str());
    }

    this->m_modelType = this->m_llmModelPtr->GetType();
    this->m_visionPromptFormat = ResolveVisionPromptFormat();
    LOG_INF("Model Loaded (type=%s)", this->m_modelType.c_str());
}

void LLM::LLMImpl::FreeModel()
{
    if (this->m_llmModelPtr)
    {
        this->m_llmModelPtr.reset();
        this->m_llmModelPtr = nullptr;
        LOG_INF("Freed Model");
    }

    this->m_modelType.clear();
    this->m_visionPromptFormat = VisionPromptFormat::None;
    this->m_llmInitialized = false;
}

void LLM::LLMImpl::LlmInit(const LlmConfig &config, const std::string &)
{
    this->FreeLlm();

    this->m_config = config;
    this->m_numOfThreads = config.GetConfigInt(LlmConfig::ConfigParam::NumThreads);
    this->m_modelPath = config.GetConfigString(LlmConfig::ConfigParam::LlmModelName);
    this->m_batchSz = config.GetConfigInt(LlmConfig::ConfigParam::BatchSize);
    this->m_nCtx = config.GetConfigInt(LlmConfig::ConfigParam::ContextSize);
    this->m_isVision = config.GetConfigBool(LlmConfig::ConfigParam::IsVision);

    try
    {
        InitConfigs();
        LoadModel();
        InitTokenizer();
        if (this->m_isVision)
        {
            InitMultiModalProcessor();
        }
        InitGenerator();
    }
    catch (const std::exception &e)
    {
        this->FreeLlm();
        THROW_ERROR("LLM initialization failed: %s", e.what());
    }

    this->m_llmInitialized = true;
    LOG_INF("LLM Initialized");
}

void LLM::LLMImpl::FreeLlm()
{
    FreeSequence();
    this->m_visionTurns.clear();
    this->m_visionReplayTokenizerStreamPtr.reset();
    FreeGenerator();
    FreeTokenizer();
    FreeMultiModalProcessor();
    FreeModel();
    FreeConfigs();

    this->m_isConversationStart = true;
    this->m_contextFilled = 0;
    this->m_nCurr = 0;
    this->m_isVision = false;
    this->m_modelPath.clear();
    ClearTimingState();
    this->m_llmInitialized = false;
}

void LLM::LLMImpl::ResetContext()
{
    InitGenerator();
    this->m_visionTurns.clear();
    this->m_visionReplayTokenizerStreamPtr.reset();

    FreeSequence();
    this->m_isConversationStart = true;
    ResetTimings();

    this->m_contextFilled = 0;
    this->m_nCurr = 0;
    LOG_INF("Reset Context");
}

void LLM::LLMImpl::QueryBuilder(LlmChat::Payload &payload)
{
    if (m_isDefaultChatTemplate || !ApplyAutoChatTemplate(payload))
    {
        if (m_isVision && !payload.imagePath.empty())
        {
            payload.textPrompt = BuildInlineVisionPrompt(
                payload.textPrompt, kSingleImageCount, NextVisionImageIndex());
        }

        ApplyDefaultChatTemplate(payload);
    }
}

bool LLM::LLMImpl::ApplyAutoChatTemplate(LlmChat::Payload &payload)
{
    if (!this->m_tokenizerPtr)
    {
        LOG_WARN("ApplyChatTemplate requested before tokenizer initialization.");
        return false;
    }

    auto chatMsg = [](std::string_view role, const nlohmann::json &content) -> nlohmann::json
    {
        return nlohmann::json{{"role", role}, {"content", content}};
    };

    nlohmann::json messages = nlohmann::json::array();

    if (m_isConversationStart)
    {
        messages.push_back(chatMsg("system", nlohmann::json(m_systemPrompt)));
    }

    messages.push_back(chatMsg("user", BuildUserContent(payload)));

    const std::string messages_json = messages.dump();

    try
    {
        const std::string formatted = std::string(
            m_tokenizerPtr->ApplyChatTemplate("", messages_json.c_str(), "", /*add_generation_prompt=*/true));

        if (formatted.empty())
        {
            LOG_WARN("ApplyChatTemplate produced empty output. Falling back to default template.");
            return false;
        }

        payload.textPrompt = formatted;
        return true;
    }
    catch (const std::exception &e)
    {
        LOG_WARN("ApplyChatTemplate failed: %s. Falling back to default template.", e.what());
        return false;
    }
}

void LLM::LLMImpl::Encode(const LlmChat::Payload &payload)
{
    const std::string prompt = payload.textPrompt;
    const bool hasVisionImage = this->m_isVision && !payload.imagePath.empty();
    const bool requiresVisionReplay = this->m_isVision &&
                                      (hasVisionImage || this->m_visionTurns.empty());

    try
    {
        if (!this->m_llmGeneratorPtr)
        {
            THROW_ERROR("ONNX Runtime GenAI generator is not initialized");
        }

        const TimePoint startTimeStampEncoder = Clock::now();

        std::vector<VisionTurn> candidateVisionTurns;
        std::unique_ptr<OgaTokenizerStream> replayTokenizerStream;
        if (this->m_isVision)
        {
            if (!this->m_tokenizerPtr)
            {
                THROW_ERROR("ONNX Runtime GenAI tokenizer is not initialized");
            }

            candidateVisionTurns = this->m_visionTurns;
            candidateVisionTurns.push_back({prompt, payload.imagePath, {}});
            replayTokenizerStream = OgaTokenizerStream::Create(*this->m_tokenizerPtr);
            if (replayTokenizerStream == nullptr)
            {
                THROW_ERROR("ONNX Runtime GenAI replay tokenizer stream is not initialized");
            }
        }

        if (requiresVisionReplay)
        {
            RebuildVisionGenerator(candidateVisionTurns);
        }
        else
        {
            if (!this->m_tokenizerPtr)
            {
                THROW_ERROR("ONNX Runtime GenAI tokenizer is not initialized");
            }
            if (!payload.imagePath.empty())
            {
                THROW_ERROR("ONNX Runtime GenAI image payload received for a text-only model");
            }

            InitSequence();
            this->m_tokenizerPtr->Encode(prompt.c_str(), *this->m_sequencesPtr);
            const size_t sequenceCount = this->m_sequencesPtr->SequenceCount(0);
            if (this->m_nCurr + sequenceCount >= static_cast<size_t>(this->m_nCtx))
            {
                THROW_ERROR("LLM encoding failed, context is full");
            }
            this->m_llmGeneratorPtr->AppendTokenSequences(*this->m_sequencesPtr);
            this->m_totalEncodedTokens += sequenceCount;
            this->m_nCurr += sequenceCount;
            this->m_contextFilled = 100 * this->m_nCurr / this->m_nCtx;
        }

        if (this->m_isVision)
        {
            this->m_visionTurns = std::move(candidateVisionTurns);
            this->m_visionReplayTokenizerStreamPtr = std::move(replayTokenizerStream);
        }

        this->m_totalEncoderTime += Duration(Clock::now() - startTimeStampEncoder).count();
        this->m_isConversationStart = false;
    }
    catch (const std::exception &e)
    {
        const std::string_view error{e.what()};
        if (error.find("exceeds max length") != std::string_view::npos)
        {
            THROW_ERROR("LLM encoding failed, context is full: %s", e.what());
        }

        THROW_ERROR("Failed to evaluate prompt: %s", e.what());
    }
}

std::vector<std::string> LLM::LLMImpl::SupportedInputModalities() const
{
    if (this->m_isVision)
    {
        return {"text", "image"};
    }
    return {"text"};
}

std::optional<LLM::TextTokenId> LLM::LLMImpl::NextTokenId()
{
    try
    {
        if (!this->m_llmGeneratorPtr->IsDone())
        {
            const TimePoint startTimeStampDecoder = Clock::now();

            this->m_llmGeneratorPtr->GenerateNextToken();
            const size_t cnt = this->m_llmGeneratorPtr->GetSequenceCount(0);
            const int32_t tok = this->m_llmGeneratorPtr->GetSequenceData(0)[cnt - 1];

            if (this->m_isVision && !this->m_visionTurns.empty())
            {
                this->m_visionTurns.back().assistantResponse +=
                    this->m_visionReplayTokenizerStreamPtr->Decode(tok);
            }

            this->m_totalDecoderTime += Duration(Clock::now() - startTimeStampDecoder).count();
            this->m_totalDecodedTokens += 1;

            this->m_nCurr += 1;
            this->m_contextFilled = 100 * this->m_nCurr / this->m_nCtx;

            m_lastTerminationReason = TerminationReason::None;
            return tok;
        }
        else
        {
            m_lastTerminationReason = TerminationReason::BackendEos;
            return std::nullopt;
        }
    }
    catch (const std::exception &e)
    {
        THROW_ERROR("Failed to decode next token : %s", e.what());
    }
    return std::nullopt;
}

std::string LLM::LLMImpl::DetokenizeTextToken(TextTokenId token)
{
    return this->m_tokenizerStreamPtr->Decode(token);
}

void LLM::LLMImpl::Cancel()
{
    LOG_INF("Cancelling current operation");
}

size_t LLM::LLMImpl::GetChatProgress() const
{
    return this->m_contextFilled;
}

float LLM::LLMImpl::GetEncodeTimings() const
{
    if (this->m_totalEncoderTime <= 0.0)
    {
        return 0.0f;
    }

    const auto encoderTPS = this->m_totalEncodedTokens / this->m_totalEncoderTime;
    return static_cast<float>(encoderTPS);
}

float LLM::LLMImpl::GetDecodeTimings() const
{
    if (this->m_totalDecoderTime <= 0.0)
    {
        return 0.0f;
    }

    const auto decoderTPS = this->m_totalDecodedTokens / this->m_totalDecoderTime;
    return static_cast<float>(decoderTPS);
}

void LLM::LLMImpl::ClearTimingState()
{
    this->m_totalDecoderTime = 0.0;
    this->m_totalEncoderTime = 0.0;
    this->m_totalDecodedTokens = 0;
    this->m_totalEncodedTokens = 0;
}

void LLM::LLMImpl::ResetTimings()
{
    ClearTimingState();
    LOG_INF("Reset Timings");
}

std::string LLM::LLMImpl::SystemInfo()
{
    std::string sysInfo = "\nSystem INFO:\n";

    if (!this->m_llmModelPtr)
    {
        sysInfo += "Model not loaded\n";
        return sysInfo;
    }

    const std::string deviceType = std::string(this->m_llmModelPtr->GetDeviceType());
    sysInfo += "Device Type: " + deviceType + "\n";
    sysInfo += "Model Type: " + this->m_modelType + "\n";
    return sysInfo;
}

std::string LLM::LLMImpl::GeneratePromptWithNumTokens(const size_t numPromptTokens)
{
    const char *const base_prompt = "A";

    auto base_prompt_sequences = OgaSequences::Create();
    m_tokenizerPtr->Encode(base_prompt, *base_prompt_sequences);

    auto params = OgaGeneratorParams::Create(*this->m_llmModelPtr);
    params->SetSearchOption("max_length", static_cast<double>(numPromptTokens));
    params->SetSearchOption("min_length", static_cast<double>(numPromptTokens));

    auto generator = OgaGenerator::Create(*this->m_llmModelPtr, *params);
    generator->AppendTokenSequences(*base_prompt_sequences);

    while (!generator->IsDone())
    {
        generator->GenerateNextToken();
    }

    const auto output_sequence_length = generator->GetSequenceCount(0);
    const auto *output_sequence_data = generator->GetSequenceData(0);

    return std::string{
        m_tokenizerPtr->Decode(output_sequence_data, output_sequence_length)};
}

void LLM::LLMImpl::StopGeneration()
{
    // TODO: add stop response to support cancelled query
}

LLM::LLMImpl::VisionPromptFormat LLM::LLMImpl::ResolveVisionPromptFormat() const
{
    if (!this->m_isVision)
    {
        return VisionPromptFormat::None;
    }

    if (this->m_modelType == "phi3v" || this->m_modelType == "phi4mm")
    {
        return VisionPromptFormat::PhiInlineTags;
    }

    if (this->m_modelType == "qwen2_5_vl" || this->m_modelType == "qwen3_vl" || this->m_modelType == "fara")
    {
        return VisionPromptFormat::VisionPadTags;
    }

    if (this->m_modelType == "gemma3")
    {
        return VisionPromptFormat::StructuredJson;
    }

    THROW_INVALID_ARGUMENT(
        "Unsupported ONNX Runtime GenAI vision model type '%s'.",
        this->m_modelType.c_str());
}

size_t LLM::LLMImpl::NextVisionImageIndex() const
{
    size_t nextImageIndex = 1;
    for (const auto& turn : this->m_visionTurns)
    {
        if (!turn.imagePath.empty())
        {
            ++nextImageIndex;
        }
    }
    return nextImageIndex;
}

std::string LLM::LLMImpl::BuildInlineVisionPrompt(const std::string &prompt,
                                                  const size_t numImages,
                                                  const size_t firstImageIndex) const
{
    if (numImages == 0 || this->m_visionPromptFormat == VisionPromptFormat::None)
    {
        return prompt;
    }

    std::string imageTags;
    switch (this->m_visionPromptFormat)
    {
    case VisionPromptFormat::PhiInlineTags:
        for (size_t i = 0; i < numImages; ++i)
        {
            imageTags += "<|image_" + std::to_string(firstImageIndex + i) + "|>\n";
        }
        return imageTags + prompt;
    case VisionPromptFormat::VisionPadTags:
        for (size_t i = 0; i < numImages; ++i)
        {
            imageTags += "<|vision_start|><|image_pad|><|vision_end|>";
        }
        return imageTags + prompt;
    case VisionPromptFormat::StructuredJson:
        THROW_INVALID_ARGUMENT(
            "Default chat template fallback does not support structured multimodal ONNX model type '%s'.",
            this->m_modelType.c_str());
    case VisionPromptFormat::None:
        break;
    }

    return prompt;
}

nlohmann::json LLM::LLMImpl::BuildUserContent(const LlmChat::Payload &payload) const
{
    if (!this->m_isVision)
    {
        return nlohmann::json(payload.textPrompt);
    }

    if (this->m_visionPromptFormat == VisionPromptFormat::StructuredJson)
    {
        nlohmann::json content = nlohmann::json::array();
        if (!payload.imagePath.empty())
        {
            content.push_back(nlohmann::json::object({{"type", "image"}}));
        }
        content.push_back(nlohmann::json::object({{"type", "text"}, {"text", payload.textPrompt}}));
        return content;
    }

    return nlohmann::json(BuildInlineVisionPrompt(payload.textPrompt,
                                                  payload.imagePath.empty() ? 0 : kSingleImageCount,
                                                  NextVisionImageIndex()));
}
