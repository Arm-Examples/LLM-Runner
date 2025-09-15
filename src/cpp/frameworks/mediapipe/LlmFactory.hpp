//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#pragma once
#include "LlmImpl.hpp"

/**
 * @brief Factory function for creating LLMImpl
 */
class LLMFactory {
public:
    LLMFactory() = default;
    ~LLMFactory() = default;
    /**
     * Method to Create the correct Impl based on the Config
     * @param config Configuration class with model's parameter and user defined parameters
     * @return A unique pointer to the created LLM implementation.
     */
     std::unique_ptr<LLM::LLMImpl> CreateLLMImpl(const LlmConfig& config);
};


