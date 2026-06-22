//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmImpl.hpp"
#include "LlmFactory.hpp"
#include "ImageUtils.hpp"
#include <stdexcept>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include "Logger.hpp"
#include "BuildInfo.hpp"
#include "LlmBridge.hpp"
#if defined(ENABLE_STREAMLINE)
#include "profiling/StreamlineLlm.hpp"
#include <thread>
#include <chrono>
#endif

namespace {

void PrepareImagePayload(LlmChat::Payload& payload, const LlmConfig& config)
{
    if (payload.imagePath.empty()) {
        return;
    }

    const auto maxInputDimension = config.GetConfigInt(LlmConfig::ConfigParam::MaxInputDimension);
    const auto imageSize = ImageUtils::ReadImageSize(payload.imagePath);
    const auto resizedImageSize = ImageUtils::ComputeResizedImageSize(imageSize, maxInputDimension);
    if (imageSize.width == resizedImageSize.width && imageSize.height == resizedImageSize.height) {
        return;
    }

    static std::atomic<uint64_t> imageCounter{0};
    const std::filesystem::path inputPath{payload.imagePath};
    const auto imageId = imageCounter.fetch_add(1, std::memory_order_relaxed);
    const auto outputPath = inputPath.parent_path() /
                            (inputPath.stem().string() + "-resized-" +
                             std::to_string(imageId) + ".png");

    payload.imagePath = ImageUtils::ResizeImageToFile(
        payload.imagePath,
        outputPath.string(),
        maxInputDimension).path;
}

} // namespace

LLM::LLM()
{
#if defined(ENABLE_STREAMLINE)
    sl::InitThreadOnce();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    sl::Scope scope(sl::CH_CONTROL, ANNOTATE_BLUE, "LLM::LLM");
#endif
}

LLM::~LLM() noexcept
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_CONTROL, ANNOTATE_DKGRAY, "LLM::~LLM");
#endif
    this->FreeLlm();
}

void LLM::LlmInit(const LlmConfig &llmConfig, std::string sharedLibraryPath)
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_INIT, ANNOTATE_BLUE, "LLM::LlmInit");
    sl::marker(ANNOTATE_BLUE, "Init start");
#endif
    LLMFactory factory;
    LlmLog::LogBuildMetadataOnce();

    std::string frameworkType = LlmLog::GetBuildMetadata().frameworkName;
    try {
        this->m_impl = factory.CreateLLMImpl(llmConfig);
        if (!this->m_impl) {
            throw std::runtime_error("Failed to create LLM implementation");
        }

        frameworkType = this->m_impl->GetFrameworkType();
        this->m_config = llmConfig;
        this->m_impl->InitChatParams(this->m_config.GetChat());

        LOG_BUILD_INFO("Initializing LLM with framework='%s'", frameworkType.c_str());
        this->m_impl->LlmInit(this->m_config, sharedLibraryPath);
        LOG_BUILD_INFO("LLM initialization complete using framework='%s'", frameworkType.c_str());
    } catch (const std::exception& e) {
        LlmLog::LogInitializationFailure(frameworkType, e.what());
        throw;
    }

#if defined(ENABLE_STREAMLINE)
    sl::marker(ANNOTATE_BLUE, "Init complete");
#endif
}

void LLM::FreeLlm()
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_CONTROL, ANNOTATE_DKGRAY, "LLM::FreeLlm");
#endif
    if (!this->m_impl) {
        return;
    }

    const std::string frameworkType = this->m_impl->GetFrameworkType();
    try {
        this->m_impl->FreeLlm();
    } catch (const std::exception& e) {
        LOG_ERROR("LLM cleanup failed using framework='%s': %s", frameworkType.c_str(), e.what());
    } catch (...) {
        LOG_ERROR("LLM cleanup failed using framework='%s': unknown error", frameworkType.c_str());
    }

    this->m_impl.reset();
}

float LLM::GetEncodeTimings() const
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_CONTROL, ANNOTATE_DKGRAY, "LLM::GetEncodeTimings");
#endif
    return this->m_impl->GetEncodeTimings();
}

float LLM::GetDecodeTimings() const
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_CONTROL, ANNOTATE_DKGRAY, "LLM::GetDecodeTimings");
#endif
    return this->m_impl->GetDecodeTimings();
}

void LLM::ResetTimings()
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_CONTROL, ANNOTATE_DKGRAY, "LLM::ResetTimings");
#endif
    this->m_impl->ResetTimings();
}

std::string LLM::SystemInfo() const
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_CONTROL, ANNOTATE_DKGRAY, "LLM::SystemInfo");
#endif
    return this->m_impl->SystemInfo();
}

void LLM::ResetContext()
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_CONTROL, ANNOTATE_DKGRAY, "LLM::ResetContext");
#endif
    this->m_impl->ResetContext();
    this->m_impl->SetLastTerminationReason(TerminationReason::None);
}

void LLM::Encode(LlmChat::Payload& payload) {
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_ENCODE, ANNOTATE_GREEN, "LLM::Encode");

    // Modality markers (rare / high-signal)
    if (!payload.textPrompt.empty() && payload.imagePath.empty()) {
        sl::marker(ANNOTATE_GREEN, "Encode: text");
    } else if (payload.textPrompt.empty() && !payload.imagePath.empty()) {
        sl::marker(ANNOTATE_CYAN, "Encode: image");
    } else if (!payload.textPrompt.empty() && !payload.imagePath.empty()) {
        sl::marker(ANNOTATE_YELLOW, "Encode: text+image");
    }
#endif

    if (!m_impl) {
        THROW_ERROR("LLM not initialized");
    }
    const std::vector<std::string> &inptMods = m_impl->SupportedInputModalities();

    if(payload.textPrompt != "") {
        bool supportsText = SupportsModality(inptMods, "text");
        if(!supportsText) {
            THROW_ERROR("Error. Attempting to Encode an unsupported Text payload");
        }
    }
    if(payload.imagePath != "") {
        bool supportsVision = SupportsModality(inptMods, "image");
        if(!supportsVision) {
            THROW_ERROR("Error. Attempting to Encode an unsupported Image payload");
        }
        PrepareImagePayload(payload, m_config);
    }
    this->m_impl->QueryBuilder(payload);
    this->m_impl->SetLastTerminationReason(TerminationReason::None);
    this->m_impl->Encode(payload);
}

bool LLM::SupportsModality(const std::vector<std::string> &inptMods, std::string modality) const {
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_CONTROL, ANNOTATE_DKGRAY, "LLM::SupportsModality");
#endif
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return s;
    };

    bool supportsText = std::any_of(inptMods.begin(),
                                    inptMods.end(),
                                    [&](const std::string& s) {
                                        return toLower(s) == modality;
                                    });
    return supportsText;
}

std::optional<LLM::TextTokenId> LLM::NextTokenId()
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_DECODE, ANNOTATE_PURPLE, "LLM::NextTokenId");
#endif

    return this->m_impl->NextTokenId();
}

std::optional<LLM::TextTokenId> LLM::CancellableNextTokenId(long operationId) const
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope outer(sl::CH_DECODE, ANNOTATE_PURPLE, "LLM::CancellableNextTokenId");
#endif

    auto state = std::make_shared<WorkState>();
    state->operationId = operationId;
    addWork(state);

    auto nextToken = this->m_impl->NextTokenId();

    auto work = removeWork(state->operationId);

    // Check for cancel
    if (work->cancelled.load(std::memory_order_acquire)) {

        // We support the option to only build the C++ bindings. This complier directive allows
        // only the C++ binding to be build and prevents the library attempting to trigger the
        // callback to the Java layer.
#if defined(JNI_FOUND)
        deliverCompletion(state->operationId, RESULT_CANCELLED, "cancelled");
#endif

        this->m_impl->SetLastTerminationReason(TerminationReason::Cancelled);
        return std::nullopt;
    }
    return nextToken;
}

void LLM::Cancel(long operationId)
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_CONTROL, ANNOTATE_RED, "LLM::Cancel");
    sl::marker(ANNOTATE_RED, "Cancel requested");
#endif

    auto state = findWork(operationId);
    if (!state) {
        return;
    }
    state->cancelled.store(true, std::memory_order_release);


    this->m_impl->Cancel();
}

size_t LLM::GetChatProgress() const
{
    return this->m_impl->GetChatProgress();
}

std::string LLM::GetFrameworkType()
{
    return LLM::LLMImpl::GetFrameworkType();
}

std::vector<std::string> LLM::SupportedInputModalities() const
{
    return this->m_impl->SupportedInputModalities();
}

std::string LLM::DetokenizeTextToken(TextTokenId token)
{
    return this->m_impl->DetokenizeTextToken(token);
}

LLM::TerminationReason LLM::GetLastTerminationReason() const
{
    return this->m_impl->GetLastTerminationReason();
}

bool LLM::IsStopTextPiece(const std::string& text) const
{
   for (const auto& stopToken: this->m_config.GetStopWords()) {
        if (text == stopToken) {
            return true;
        }
    }

    return false;
}

std::string LLM::GeneratePromptWithNumTokens(size_t numPromptTokens)
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_ENCODE, ANNOTATE_GREEN, "LLM::GeneratePromptWithNumTokens");
#endif
    return this->m_impl->GeneratePromptWithNumTokens(numPromptTokens);
}

void LLM::StopGeneration()
{
#if defined(ENABLE_STREAMLINE)
    sl::Scope scope(sl::CH_CONTROL, ANNOTATE_RED, "LLM::StopGeneration");
    sl::marker(ANNOTATE_RED, "StopGeneration requested");
#endif
    this->m_impl->StopGeneration();
}
