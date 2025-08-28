//
// SPDX-FileCopyrightText: Copyright 2024-2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef ARM_LLM_HPP
#define ARM_LLM_HPP


#include "LlmConfig.hpp"
#include <memory>
#include <atomic>

/**
 * @class LLM
 * @brief Interface class for interacting with a Large Language Model.
 */
class LLM {

public:

    /**
     * The token used to signify the end of a response or generation.
     */
    const std::string endToken{"<eos>"};

private:
    LlmConfig m_config;
    class LLMImpl;
    std::unique_ptr<LLMImpl> m_impl{};
    std::atomic<bool> m_evaluatedOnce{false};
    std::string m_streamEndFlag;
    std::string m_tokenBuffer;
    int m_maxStopWordLength{};
    bool m_stopFlag{};

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

    /**
     * Method to get framework type
     * @return string framework type
     */
    std::string GetFrameworkType();

    /**
    * Method to Cancel generation of response tokens. Can be used to stop response once query commences
    */
    void StopGeneration();

private:
    /**
    * Method to format prompt into a style model understands conversation
    * @param prompt raw prompt string
    * @return formatted query
    */
    std::string QueryBuilder(std::string &prompt);
    /**
     * Method to detect stop words in internal token buffer and emit correct output
     * @return token string up to stop word, end token, or partial output
     */
    std::string ContainsStopWord();

};

#endif /* ARM_LLM_HPP */
