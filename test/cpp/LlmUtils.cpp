//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#include "LlmUtils.hpp"
#include <fstream>
#include <iostream>

namespace Llm::Test::Utils {

std::unordered_map<std::string, std::string> LoadConfig(const std::string& configFilePath)
{
    std::unordered_map<std::string, std::string> config;
    std::ifstream file(configFilePath);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + configFilePath);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue; // Skip empty lines and comments

        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key   = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);
            config[key]       = value;
        }
    }

    return config;
}

std::unordered_map<std::string, int> LoadUserConfig(const std::string& userConfigFilePath)
{
    std::unordered_map<std::string, int> config;
    std::ifstream file(userConfigFilePath);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + userConfigFilePath);
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#')
            continue; // Skip empty lines and comments

        size_t delimiterPos = line.find('=');
        if (delimiterPos != std::string::npos) {
            std::string key   = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);
            // sanitize the numerical values
            try {
                size_t numSize;
                int numericalValue = std::stoi(value, &numSize);
                // ensure only numbers are present
                if (numSize != value.length()) {
                    throw std::invalid_argument("Extra characters after number");
                }
                config[key] = numericalValue;
            } catch (const std::invalid_argument& e) {
                std::cerr << "Invalid input: " << value << " for " << key << "\n";
            } catch (const std::out_of_range& e) {
                std::cerr << "Out of range input: " << value << " for " << key << "\n";
            }
        }
    }

    return config;
}

LlmConfig GetConfig(std::unordered_map<std::string, std::string> config,
                    std::unordered_map<std::string, int> userConfig)
{
    if (config.find("modelPath") == config.end())
        throw std::runtime_error("Missing required parameter: modelPath");
    if (config.find("modelTag") == config.end())
        throw std::runtime_error("Missing required parameter: modelTag");
    if (config.find("llmPrefix") == config.end())
        throw std::runtime_error("Missing required parameter: llmPrefix");

    if (userConfig.find("batchSize") == userConfig.end())
        throw std::runtime_error("Missing required parameter: batchSize");
    if (userConfig.find("numThreads") == userConfig.end())
        throw std::runtime_error("Missing required parameter: numThreads");
    if (config.find("stopWords") == config.end())
        throw std::runtime_error("Missing required parameter: stopWords");

    return LlmConfig(config.at("modelTag"),
                     config.at("modelPath"),
                     config.at("llmPrefix"),
                     userConfig.at("numThreads"),
                     userConfig.at("batchSize"));
}

} /* namespace Llm::Test::Utils */
