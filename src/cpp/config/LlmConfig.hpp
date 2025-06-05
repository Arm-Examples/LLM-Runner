//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef LLM_CONFIG_HPP
#define LLM_CONFIG_HPP

#include <string>

/**
 * @class LlmConfig
 * @brief Config class for the Large Language Model settings.
 */
class LlmConfig {
private:
    std::string m_modelTag{};
    std::string m_modelPath{};
    std::string m_llmPrefix{};
    int m_numThreads{};
    int m_batchSize{};

public:
    /**
     * LlmConfig
     * @param modelTag    Model tag for the LLM model
     * @param modelPath   Path to the model
     * @param llmPrefix   LLM prefix to use
     * @param numThreads  Number of threads to use
     * @param batchSize   Batch size to use
     */
    LlmConfig(const std::string& modelTag,
              const std::string& modelPath,
              const std::string& llmPrefix,
              int numThreads,
              int batchSize);

    LlmConfig() = default;

    /**
     * Returns the model tag string (The name to appear in conversation with the LLM).
     * @return modelTag
     */
    std::string GetModelTag() const;

    /**
     * Returns the path to the model file.
     * @return modelPath
     */
    std::string GetModelPath() const;

    /**
     * Returns the LLM prompt prefix string.
     * @return llmPrefix
     */
    std::string GetLlmPrefix() const;

    /**
     * Returns the number of threads configured for inference.
     * @return number of Threads
     */
    int GetNumThreads() const;

    /**
     * Returns the batch size used for querying.
     * @return batch size
     */
    int GetBatchSize() const;

    /**
     * Sets the model tag (The name to appear in conversation with the LLM).
     * @param modelIdentifier is the tag name added at the end of each user question to make model
     * respond appropriately
     */
    void SetModelTag(const std::string& modelIdentifier);

    /**
     * Sets the file path to the model.
     * @param basePath absolute path to load llm model
     */
    void SetModelPath(const std::string& basePath);

    /**
     * Method sets the prompt prefix used for LLM inputs.
     * @param llmInitialPrompt LLM's need to prompt engineered to respond intelligently.
     * Provide an engineered initial-prompt here.
     */
    void SetLlmPrefix(const std::string& llmInitialPrompt);

    /**
     * Sets the number of threads to use for LLM model inference.
     * @param threads number of threads used inference of model
     */
    void SetNumThreads(int threads);

    /**
     * Sets the batch size for inference. Throws std::invalid_argument if the value is not positive.
     * @param batchSz chunk-size of each batch used to split query-encoding
     */
    void SetBatchSize(int batchSz);
};

#endif /* LLM_CONFIG_HPP */
