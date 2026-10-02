//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "BenchScenario.hpp"

#include "ImageUtils.hpp"
#include "Logger.hpp"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

using nlohmann::json;

namespace {

std::string ReadFileToString(const std::filesystem::path& path)
{
    std::ifstream in(path);
    if (!in) {
        THROW_INVALID_ARGUMENT("Failed to open scenario JSON file: %s", path.string().c_str());
    }

    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::optional<std::string> ReadOptionalString(const json& value, const char* key)
{
    if (!value.contains(key) || value.at(key).is_null()) {
        return std::nullopt;
    }
    if (!value.at(key).is_string()) {
        THROW_INVALID_ARGUMENT("scenario.%s must be a string", key);
    }

    std::string out = value.at(key).get<std::string>();
    if (out.empty()) {
        return std::nullopt;
    }
    return out;
}

} // namespace

bool BenchScenario::HasImage() const
{
    return imagePath.has_value() && !imagePath->empty();
}

bool BenchScenario::HasPrompt() const
{
    return prompt.has_value() && !prompt->empty();
}

int BenchScenario::ImageCount() const
{
    return HasImage() ? 1 : 0;
}

std::string BenchScenario::WorkloadType() const
{
    if (HasImage() && HasPrompt()) {
        return "image_text";
    }
    if (HasImage()) {
        return "image";
    }
    return "text";
}

BenchScenario LoadBenchScenarioFromJson(const std::string& path)
{
    const std::filesystem::path scenarioPath(path);
    if (!std::filesystem::exists(scenarioPath)) {
        THROW_INVALID_ARGUMENT("Scenario JSON file does not exist: %s", path.c_str());
    }
    if (!std::filesystem::is_regular_file(scenarioPath)) {
        THROW_INVALID_ARGUMENT("Scenario path is not a regular file: %s", path.c_str());
    }

    json root;
    try {
        root = json::parse(ReadFileToString(scenarioPath));
    } catch (const nlohmann::json::exception& e) {
        THROW_INVALID_ARGUMENT("Scenario JSON is invalid: %s", e.what());
    }
    if (!root.is_object()) {
        THROW_INVALID_ARGUMENT("Scenario JSON must be an object");
    }

    BenchScenario scenario{};
    const auto image = ReadOptionalString(root, "image");
    const auto imagePath = ReadOptionalString(root, "image_path");
    if (image.has_value() && imagePath.has_value() && image.value() != imagePath.value()) {
        THROW_INVALID_ARGUMENT("Scenario JSON must not specify different values for image and image_path");
    }

    scenario.imagePath = imagePath.has_value() ? imagePath : image;
    scenario.prompt = ReadOptionalString(root, "prompt");

    if (root.contains("max_output_tokens") && !root.at("max_output_tokens").is_null()) {
        if (!root.at("max_output_tokens").is_number_integer()) {
            THROW_INVALID_ARGUMENT("scenario.max_output_tokens must be an integer");
        }
        const int maxOutputTokens = root.at("max_output_tokens").get<int>();
        if (maxOutputTokens <= 0) {
            THROW_INVALID_ARGUMENT("scenario.max_output_tokens must be > 0");
        }
        scenario.maxOutputTokens = maxOutputTokens;
    }

    if (!scenario.HasImage() && !scenario.HasPrompt()) {
        THROW_INVALID_ARGUMENT("Scenario JSON must provide at least one of image/image_path or prompt");
    }

    if (scenario.HasImage()) {
        std::filesystem::path resolvedImagePath(*scenario.imagePath);
        if (resolvedImagePath.is_relative()) {
            const std::filesystem::path scenarioDir = scenarioPath.parent_path();
            if (!scenarioDir.empty()) {
                resolvedImagePath = scenarioDir / resolvedImagePath;
            }
        }
        resolvedImagePath = resolvedImagePath.lexically_normal();
        if (!std::filesystem::exists(resolvedImagePath)) {
            THROW_INVALID_ARGUMENT("Scenario image file does not exist: %s", resolvedImagePath.string().c_str());
        }
        if (!std::filesystem::is_regular_file(resolvedImagePath)) {
            THROW_INVALID_ARGUMENT("Scenario image path is not a regular file: %s", resolvedImagePath.string().c_str());
        }

        (void)ImageUtils::ReadImageSize(resolvedImagePath.string());
        scenario.imagePath = resolvedImagePath.string();
    }

    return scenario;
}
