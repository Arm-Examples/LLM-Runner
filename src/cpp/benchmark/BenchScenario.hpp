//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef LLM_BENCH_SCENARIO_HPP
#define LLM_BENCH_SCENARIO_HPP

#include <optional>
#include <string>

/**
 * @struct BenchScenario
 * @brief User-provided benchmark payload for scenario mode.
 *
 * A scenario describes the input payload supplied through `--scenario`.
 * It can represent text-only, image-only, or image-plus-text workloads.
 * Relative image paths are resolved by the parser before this struct is
 * returned to benchmark code.
 */
struct BenchScenario {
    std::optional<std::string> imagePath{}; ///< Resolved image path, if the scenario includes an image.
    std::optional<std::string> prompt{}; ///< Optional text prompt supplied alongside the image or alone.
    std::optional<int> maxOutputTokens{}; ///< Optional scenario-specific decode token limit.

    /**
     * @brief Check whether the scenario includes an image payload.
     * @return true when imagePath is present.
     */
    [[nodiscard]] bool HasImage() const;

    /**
     * @brief Check whether the scenario includes prompt text.
     * @return true when prompt is present.
     */
    [[nodiscard]] bool HasPrompt() const;

    /**
     * @brief Return the number of images in the scenario.
     * @return 1 for image scenarios, otherwise 0.
     */
    [[nodiscard]] int ImageCount() const;

    /**
     * @brief Return a stable workload label for report output.
     * @return "image_text", "image", or "text".
     */
    [[nodiscard]] std::string WorkloadType() const;
};

/**
 * @brief Parse a benchmark scenario JSON file and validate its payload.
 *
 * Supports `image` or `image_path`, optional `prompt`, and optional
 * `max_output_tokens`. Relative image paths are resolved from the scenario
 * file directory.
 *
 * @param path Path to the scenario JSON file.
 * @return Validated benchmark scenario with resolved paths.
 * @throws std::invalid_argument if the file, JSON schema, or payload is invalid.
 */
BenchScenario LoadBenchScenarioFromJson(const std::string& path);

#endif /* LLM_BENCH_SCENARIO_HPP */
