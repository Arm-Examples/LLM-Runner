//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef ARM_LLM_HPP
#define ARM_LLM_HPP

#include "LlmConfig.hpp"
#include <memory>

class LLM {
private:
    class LLMImpl;
    std::unique_ptr<LLMImpl> m_impl{};

public:
    LLM();  /**< Constructor */
    ~LLM(); /**< Destructor */

    /**
     * Method to Initialize a llama_model
     * @param llmConfig Configuration class with model's parameter and user defined parameters
     */
    void LlmInit(const LlmConfig& llmConfig);

    /**
     * Method to Free Model
     */
    void FreeLlm();

    /**
     * Function to retrieve the llm encode timings
     * @return encode timings
     */
    float GetEncodeTimings();

    /**
     * Function to retrieve the llm decode timings
     * @return decode timings
     */
    float GetDecodeTimings();

    /**
     * Function to reset the llm timings
     */
    void ResetTimings();

    /**
     * Function to print the system info
     * @return System info as a char pointer
     */
    std::string SystemInfo();

    /**
     * Method to reset Conversation history and preserve Model's character prefix.
     * If model's prefix is not defined all conversation history would be cleared
     */
    void ResetContext();

    /**
     * Function to Encode Query into the llm. Use NextToken to get subsequent tokens.
     * @param text THe query to be encoded
     */
    void Encode(std::string text);

    /**
     * Function to get response from llm as token by token. Call Encode before
     * @return result single token
     */
    std::string NextToken();

    /**
     * Function to get percentage of Context capacity filled in model's cache
     * @return percentage of context filled
     */
    size_t GetChatProgress();

    /**
     * Function to bench the underlying llm backend
     * @param nPrompts prompt length used for benchmarking
     * @param nEvalPrompts number of generated tokens for benchmarking
     * @param nMaxSeq sequence number
     * @param nRep number of repetitions
     * @return the results of benchmarking in string format for prompt generation and evaluation
     */

    std::string BenchModel(int& nPrompts, int& nEvalPrompts, int& nMaxSeq, int& nRep);
};

#endif /* ARM_LLM_HPP */
