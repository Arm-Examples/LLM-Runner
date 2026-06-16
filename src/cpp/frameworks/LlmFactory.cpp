//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmFactory.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <cctype>

#if defined(LLM_BACKEND_LLAMA_CPP)
#include "llama_cpp/LlamaTextImpl.hpp"
#include "llama_cpp/LlamaVisionImpl.hpp"
#endif

#if defined(LLM_BACKEND_ONNXRUNTIME_GENAI)
#include "onnxruntime_genai/LlmImpl.hpp"
#endif

#if defined(LLM_BACKEND_MNN)
#include "mnn/LlmImpl.hpp"
#endif

#if defined(LLM_BACKEND_MEDIAPIPE)
#include "mediapipe/LlmImpl.hpp"
#endif

#if defined(LLM_BACKEND_EXECUTORCH)
#include "executorch/LlmImpl.hpp"
#endif
static std::string NormalizeFramework(std::string framework) {
    std::transform(framework.begin(), framework.end(), framework.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return framework;
}

std::unique_ptr<LLM::LLMImpl> LLMFactory::CreateLLMImpl(const LlmConfig &config) {
    std::string framework = config.GetFramework();
    if (framework.empty()) {
        THROW_INVALID_ARGUMENT("config.framework is required");
    }
    framework = NormalizeFramework(framework);

    if (framework == "llama.cpp") {
#if defined(LLM_BACKEND_LLAMA_CPP)
        if (config.GetConfigBool(LlmConfig::ConfigParam::IsVision)) {
            return std::make_unique<LlamaVisionImpl>();
        }
        return std::make_unique<LlamaTextImpl>();
#else
        THROW_INVALID_ARGUMENT(
            "llama.cpp backend configured but not included in this build; enable LLM_ENABLE_LLAMA_CPP "
            "or select another backend");
#endif
    }

    if (framework == "onnxruntime-genai") {
#if defined(LLM_BACKEND_ONNXRUNTIME_GENAI)
        return std::make_unique<OnnxrtImpl>();
#else
        THROW_INVALID_ARGUMENT(
            "onnxruntime-genai backend configured but not included in this build; enable "
            "LLM_ENABLE_ONNXRUNTIME_GENAI or select another backend");
#endif
    }

    if (framework == "mnn") {
#if defined(LLM_BACKEND_MNN)
        return std::make_unique<MnnImpl>();
#else
        THROW_INVALID_ARGUMENT(
            "mnn backend configured but not included in this build; enable LLM_ENABLE_MNN "
            "or select another backend");
#endif
    }

    if (framework == "mediapipe") {
#if defined(LLM_BACKEND_MEDIAPIPE)
        return std::make_unique<MediaPipeImpl>();
#else
        THROW_INVALID_ARGUMENT(
            "mediapipe backend configured but not included in this build; enable LLM_ENABLE_MEDIAPIPE "
            "or select another backend");
#endif
    }

    if (framework == "executorch") {
#if defined(LLM_BACKEND_EXECUTORCH)
        return std::make_unique<ExecuTorchImpl>();
#else
        THROW_INVALID_ARGUMENT(
            "executorch backend configured but not included in this build; enable LLM_ENABLE_EXECUTORCH "
            "or select another backend");
#endif
    }

    THROW_INVALID_ARGUMENT("Framework '%s' is not available in this build", framework.c_str());
}
