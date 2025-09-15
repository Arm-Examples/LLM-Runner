//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef LLM_CONFIG_HPP
#define LLM_CONFIG_HPP

#include <string>
#include <vector>

/**
 * @class LlmConfig
 * @brief Config class for the Large Language Model settings.
 */
class LlmConfig {
private:
    std::string m_modelTag{};
    std::string m_mediaTag{};
    std::string m_userTag{};
    std::string m_endTag{};
    std::string m_modelPath{};
    std::string m_mmProjModelPath{};
    std::string m_llmPrefix{};
    std::vector<std::string> m_inputModalities;
    std::vector<std::string> m_outputModalities;
    std::string m_framework;
    std::vector<std::string> m_stopWords;
    int m_numThreads{};
    int m_batchSize{};

    template <typename Container, typename T>
    bool Contains(const Container& container, const T& value);

public:
    /**
    * Constructs an LlmConfig object from a parsed JSON configuration.
    * @param jsonStr JSON string containing configuration keys and values.
    */
    LlmConfig(const std::string &jsonStr);

    LlmConfig() =default;

    /**
     * Returns the end tag string.
     * @return endTag
     */
    std::string GetEndTag() const;

    /**
     * Returns the user tag string.
     * @return userTag
     */
    std::string GetUserTag() const;

    /**
     * Returns the model tag string (The name to appear in conversation with the LLM).
     * @return modelTag
     */
    std::string GetModelTag() const;

    /**
     * Returns the media tag string.
     * @return modelTag
     */
    std::string GetMediaTag() const;

    /**
     * Returns the path to the projection model file.
     * @return modelPath
     */
    std::string GetMMPROJModelPath() const;

    /**
     * Returns the path to the model file.
     * @return modelPath
     */
    std::string GetModelPath() const;

    /**
     * Returns the path to the projection model file.
     * @return modelPath
     */
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
     * Returns stop words of llm from the config
     * @return  vector of stop words (strings)
     */
    std::vector<std::string> GetStopWords() const;

    /**
     * Get the supported input modalities for the LLM framework.
     *
     * @return A vector of input modality names (e.g., "text", "image").
     */
    std::vector<std::string> GetInputModalities() const;

    /**
     * Get the supported output modalities for the LLM framework.
     *
     * @return A vector of output modality names (e.g., "text").
     */
    std::vector<std::string> GetOutputModalities() const;

    /**
     * Sets the model tag (The name to appear in conversation with the LLM).
     * @param modelIdentifier is the tag name added at the end of each user question to make model
     * respond appropriately
     */
    void SetModelTag(const std::string& modelIdentifier);

    /**
     * Sets the user tag
     * @param userTag is the user tag added at the beginning of each user question to make model
     * respond appropriately
     */
    void SetUserTag(const std::string& userTag);

    /**
     * Sets the end tag
     * @param endTag is the end tag added at the end of each user question to make model
     * respond appropriately
     */
    void SetEndTag(const std::string& endTag);

    /**
     * Sets the file path to the model.
     * @param basePath absolute path to load llm model
     */
    void SetModelPath(const std::string& basePath);

    /**
     * Sets the file path to the model.
     * @param basePath absolute path to load llm projection model
     */
    void SetMMPROJModelPath(const std::string& basePath);

    /**
     * Sets the framework backend to be used by the LLM.
     *
     * @param framework Name of the framework (e.g., "llama.cpp", "onnxruntime-genai").
     */
    void SetFramework(const std::string& framework);

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

    /**
     * Sets the Stop words in config
     * @param stopWords is the vector of stop words
     */
    void SetStopWords(const std::vector<std::string>& stopWords);

    /**
     * Sets the supported input modalities for the LLM framework.
     *
     * @param inputModalities Vector of input modality names (e.g., "text", "image").
     */
    void SetInputModalities(const std::vector<std::string>& inputModalities);

    /**
     * Sets the supported output modalities for the LLM framework.
     *
     * @param outputModalities Vector of output modality names (e.g., "text").
     */
    void SetOutputModalities(const std::vector<std::string>& outputModalities);

    /**
    * Clears all the stop words
    */
    void ClearStopWords();

    /**
     * Method to append stop word to existing list.
     * @param stopWord stop word to be added to the existing list of stop-words
     */
    void AddStopWord(const std::string& stopWord);

    /**
     * Method to append an input modality to existing list.
     * @param inputModality the new modality to add to the list
     */
    void AddInputModality(const std::string& inputModality);

    /**
    * Method to append an output modality to existing list.
    * @param outputModality the new modality to add to the list
    */
    void AddOutputModality(const std::string& outputModality);


};

#endif /* LLM_CONFIG_HPP */
