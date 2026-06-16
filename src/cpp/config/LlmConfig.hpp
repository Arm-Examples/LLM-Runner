//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef LLM_CONFIG_HPP
#define LLM_CONFIG_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using nlohmann::json;

/**
 * @struct ChatParams
 * @brief Defines all parameters related to chat behavior and templating.
 *
 * Contains the system and user message templates, system prompt text,
 * and a flag to determine whether the default chat template is applied.
 */
struct ChatParams {
    std::string systemPrompt;               ///< System prompt text that defines assistant behavior.
    bool        applyDefaultChatTemplate;   ///< Whether to apply the built-in chat template automatically.
    std::string systemTemplate;             ///< Template string used to render the system message.
    std::string userTemplate;               ///< Template string used to render user input messages.
};

/// Enables JSON serialization and deserialization for ChatParams.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChatParams, systemPrompt, applyDefaultChatTemplate, systemTemplate, userTemplate)

/**
 * @struct RuntimeParams
 * @brief Configuration parameters related to runtime execution.
 *
 * Defines the runtime threading and batching behavior for model inference.
 */
struct RuntimeParams {
    int numThreads;                         ///< Number of threads to use for model execution.
    int batchSize;                          ///< Number of samples processed in each inference batch.
    int contextSize;                        ///< Maximum context length
};

/// Enables JSON serialization and deserialization for RuntimeParams.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(RuntimeParams, numThreads, batchSize, contextSize)

/**
 * @struct ModelParams
 * @brief Parameters defining model-related configuration.
 *
 * Includes the model file name, vision model flag, and an optional projection model path.
 */
struct ModelParams {
    std::string llmModelName;               ///< Name or path of the LLM model file to load.
    bool        isVision;                   ///< Indicates if the model supports multimodal (vision) inputs.
    std::string projModelName = "";         ///< Optional projection model name or path (if used).
    int         maxInputDimension = 128;    ///< Maximum width or height used when resizing image inputs.
};

/// Enables JSON serialization and deserialization for ModelParams.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ModelParams, llmModelName, isVision, projModelName, maxInputDimension)

/**
 * @class LlmConfig
 * @brief Manages parsing, validation, and access to LLM configuration parameters.
 * The LlmConfig class loads and validates a JSON-based configuration file,
 * providing structured access to chat, runtime, model, and stop-word parameters.
 */
class LlmConfig {
private:
    ChatParams               m_chat{};      ///< Chat-related configuration block.
    RuntimeParams            m_runtime{};   ///< Runtime configuration block.
    ModelParams              m_model{};     ///< Model configuration block.
    std::vector<std::string> m_stopWords;   ///< List of stop words used by the tokenizer or model.
    std::string              m_framework{}; ///< Framework identifier (optional).

public:
    /**
    * Constructs an LlmConfig object from a parsed JSON configuration.
    * @param jsonStr JSON string containing configuration keys and values.
    */
    explicit LlmConfig(const std::string &jsonStr);

    /**
     * @brief Default constructor (unused, provided for completeness).
     */
    LlmConfig() =default;

    /**
     * @enum ConfigParam
     * @brief Lists configuration keys for the LLM module.
     */
    enum ConfigParam {
        // String parameters
        SystemPrompt = 0,             ///< Base system prompt for all chats
        SystemTemplate = 1,           ///< Template for system messages
        UserTemplate = 2,             ///< Template for user messages
        LlmModelName = 3,             ///< Name or path of the LLM model
        ProjModelName = 4,            ///< Name or path of the projection (adapter) model
        Framework = 5,                ///< Framework identifier

        // Boolean parameters
        ApplyDefaultChatTemplate = 6, ///< Whether to use the default chat formatting
        IsVision = 7,                 ///< True if model supports vision input

        // Integer parameters
        NumThreads = 8,               ///< Number of CPU threads used for inference
        BatchSize = 9,                ///< Number of tokens per batch
        ContextSize = 10,             ///< Context window (max token limit)
        MaxInputDimension = 11,       ///< Maximum width or height used when resizing image inputs
    };

     /**
     * @brief Updates a string parameter by key name.
     * @param key   Name of the configuration parameter.
     * @param value New string value to assign.
     * @throws std::invalid_argument if the key is unknown or type mismatched.
     */
    void SetConfigString(ConfigParam key, const std::string& value);

    /**
     * @brief Updates a boolean parameter by key name.
     * @param key   Name of the configuration parameter.
     * @param value Boolean value to assign.
     * @throws std::invalid_argument if the key is unknown or type mismatched.
     */
    void SetConfigBool(ConfigParam key, bool value);

    /**
     * @brief Updates an integer parameter by key name.
     * @param key   Name of the configuration parameter.
     * @param value Integer value to assign.
     * @throws std::invalid_argument if the key is unknown or type mismatched.
     */
    void SetConfigInt(ConfigParam key, int value);

    /**
     * @brief Retrieves a string parameter by key name.
     * @param key Name of the configuration parameter.
     * @return String value corresponding to the key (empty string if optional and unset).
     * @throws std::invalid_argument if the key is unknown or not a string.
     */
    std::string GetConfigString(ConfigParam key) const;

    /**
     * @brief Retrieves a boolean parameter by key name.
     * @param key Name of the configuration parameter.
     * @return Boolean value corresponding to the key.
     * @throws std::invalid_argument if the key is unknown or not a boolean.
     */
    bool GetConfigBool(ConfigParam key) const;

    /**
     * @brief Retrieves an integer parameter by key name.
     * @param key Name of the configuration parameter.
     * @return Integer value corresponding to the key.
     * @throws std::invalid_argument if the key is unknown or not an integer.
     */
    int GetConfigInt(ConfigParam key) const;

    /**
     * @brief Accessor for chat configuration parameters.
     * @return Constant reference to the ChatParams structure.
     */
    const ChatParams& GetChat() const { return m_chat; }

    /**
     * @brief Accessor for runtime configuration parameters.
     * @return Constant reference to the RuntimeParams structure.
     */
    const RuntimeParams& GetRuntime() const { return m_runtime; }

    /**
     * @brief Accessor for model configuration parameters.
     * @return Constant reference to the ModelParams structure.
     */
    const ModelParams& GetModel() const { return m_model; }

    /**
     * @brief Retrieves the list of configured stop words.
     * @return Constant reference to the stop word vector.
     */
    const std::vector<std::string>& GetStopWords() const {return m_stopWords; }

    /**
     * @brief Retrieves the framework identifier, if set.
     * @return Framework identifier string (empty if not set).
     */
    const std::string& GetFramework() const { return m_framework; }

    /**
     * @brief Sets the framework identifier.
     * @param framework Framework identifier string.
     */
    void SetFramework(const std::string& framework) { m_framework = framework; }

    /**
     * @brief Sets the Stop words in config
     * @param stopWords is the vector of stop words
     */
    void SetStopWords(const std::vector<std::string>& stopWords);
};

#endif /* LLM_CONFIG_HPP */
