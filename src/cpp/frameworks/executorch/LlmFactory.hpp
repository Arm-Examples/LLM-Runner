//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef LLM_FACTORY_HPP
#define LLM_FACTORY_HPP

#include <memory>

#include "LlmImpl.hpp"

/**
 * @brief Factory function for creating the ExecuTorch LLM implementation.
 */
class LLMFactory {
public:
    LLMFactory() = default;
    ~LLMFactory() = default;

    std::unique_ptr<LLM::LLMImpl> CreateLLMImpl(const LlmConfig& config);
};

#endif /* LLM_FACTORY_HPP */
