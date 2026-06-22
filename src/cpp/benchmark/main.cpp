//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "BenchRunner.hpp"
#include "BenchScenario.hpp"
#include "LlmBench.hpp"
#include "Logger.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

static void PrintUsage(const char* prog)
{
    std::cerr << "\nLLM Benchmark Tool\n";
    std::cerr << "Usage:\n";
    std::cerr << "  " << prog
              << " --model <model_path>"
              << " --input <tokens>"
              << " --output <tokens>"
              << " --threads <n>"
              << " --iterations <n>"
              << " [--scenario <json>]"
              << " [--model-root <dir>]"
              << " [--context <tokens>]"
              << " [--json-output <path>]"
              << " [--warmup <n>] [--help]\n\n";

    std::cerr << "Options:\n";
    std::cerr << "  --model,       -m    Path to LLM model config/file\n";
    std::cerr << "  --input,       -i    Number of input tokens for benchmark\n";
    std::cerr << "  --output,      -o    Number of output tokens to generate\n";
    std::cerr << "  --context,     -c    Context length (tokens), power of two (default: 2048)\n";
    std::cerr << "  --threads,     -t    Number of runtime threads\n";
    std::cerr << "  --iterations,  -n    Number of benchmark iterations (default: 5)\n";
    std::cerr << "  --warmup,      -w    Number of warm-up iterations (default: 1)\n";
    std::cerr << "  --json-output, -J    Write benchmark results to JSON file\n";
    std::cerr << "  --help,        -h    Show this help message and exit\n\n";

    std::cerr << "Example:\n";
    std::cerr << "  " << prog
              << " --model models/llama3.gguf"
              << " --input 128 --output 128"
              << " --context 2048"
              << " --threads 4 --iterations 5 --warmup 2\n\n";
}

int main(int argc, char** argv)
{
    // Show help immediately if no args or help flag appears
    if (argc == 1) {
        PrintUsage(argv[0]);
        return 0;
    }

    std::string modelPath;
    std::string jsonOutputPath;
    std::string scenarioPath;
    std::string modelRootPath;
    int numInputTokens   = 0;
    int numOutputTokens  = 0;
    int numThreads       = 0;
    int contextSize      = 2048;
    int numIterations    = 5;   // default num of iterations
    int numWarmup        = 1;   // default warm-up


    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        // Help flags
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        }

        auto requireValue = [&](const std::string& name) {
                if (i + 1 >= argc) {
                LOG_ERROR("Missing value for argument: %s", name.c_str());
                PrintUsage(argv[0]);
                std::exit(1);
            }
        };

        auto parseIntArg = [&](const std::string& name) -> int {
            requireValue(name);
            try {
                std::size_t consumed = 0;
                const std::string valueStr = argv[i + 1];
                int value = std::stoi(valueStr, &consumed, 10);
                if (consumed != valueStr.size()) {
                    throw std::invalid_argument("Trailing characters");
                }
                ++i; // consume value
                return value;
            } catch (const std::exception&) {
                LOG_ERROR("Invalid integer value for argument: %s", name.c_str());
                PrintUsage(argv[0]);
                std::exit(1);
            }
        };

        if (arg == "--model" || arg == "-m") {
            requireValue(arg);
            modelPath = argv[++i];
        }
        else if (arg == "--model-root") {
            requireValue(arg);
            modelRootPath = argv[++i];
        }
        else if (arg == "--input" || arg == "-i") {
            numInputTokens = parseIntArg(arg);
        }
        else if (arg == "--output" || arg == "-o") {
            numOutputTokens = parseIntArg(arg);
        }
        else if (arg == "--context" || arg == "--context-size" || arg == "-c") {
            contextSize = parseIntArg(arg);
                auto isPowerOfTwo = [](const int value) {
                    return value > 0 && (value & (value - 1)) == 0;
                };
            if (!isPowerOfTwo(contextSize)) {
                LOG_ERROR("Invalid context length: %d", contextSize);
                LOG_ERROR("Context length must be a positive power of two.");
                return 1;
            }
        }
        else if (arg == "--threads" || arg == "-t") {
            numThreads = parseIntArg(arg);
        }
        else if (arg == "--iterations" || arg == "-n") {
            numIterations = parseIntArg(arg);
        }
        else if (arg == "--scenario" || arg == "-s") {
            requireValue(arg);
            scenarioPath = argv[++i];
        }
        else if (arg == "--warmup" || arg == "-w") {
            numWarmup = parseIntArg(arg);
        }
        else if (arg == "--json-output" || arg == "--json_output" || arg == "-J") {
            requireValue(arg);
            jsonOutputPath = argv[++i];
        }
        else {
            LOG_ERROR("Unknown or incomplete argument: %s", arg.c_str());
            PrintUsage(argv[0]);
            return 1;
        }
    }

    std::optional<BenchScenario> scenario;
    if (!scenarioPath.empty()) {
        try {
            // Scenario mode builds the benchmark payload from JSON.
            scenario = LoadBenchScenarioFromJson(scenarioPath);
            if (scenario->maxOutputTokens.has_value()) {
                numOutputTokens = scenario->maxOutputTokens.value();
            }
        } catch (const std::exception& ex) {
            LOG_ERROR("Failed to load benchmark scenario: %s", ex.what());
            return 1;
        }
    }

    // Match the test harness convention for wrapper config JSON model paths.
    if (modelRootPath.empty()) {
        const std::filesystem::path defaultModelRoot = std::filesystem::path("resources_downloaded") / "models";
        if (std::filesystem::exists(defaultModelRoot)) {
            modelRootPath = defaultModelRoot.string();
        }
    }

    if (!modelRootPath.empty() && !std::filesystem::is_directory(modelRootPath)) {
        LOG_ERROR("Error: model root directory does not exist: %s", modelRootPath.c_str());
        return 1;
    }

    // Basic validation
    const bool scenarioMode = scenario.has_value();
    if (modelPath.empty() ||
        (!scenarioMode && numInputTokens <= 0) ||
        numOutputTokens <= 0 ||
        numThreads <= 0 ||
        numIterations <= 0 ||
        numWarmup < 0) {

        LOG_ERROR("Error: Missing or invalid arguments.");
        PrintUsage(argv[0]);
        return 1;
    }

    if (!jsonOutputPath.empty()) {
        const std::filesystem::path outputPath(jsonOutputPath);
        const std::filesystem::path outputDir = outputPath.parent_path();
        if (!outputDir.empty() && (!std::filesystem::exists(outputDir) || !std::filesystem::is_directory(outputDir))) {
            LOG_ERROR("Error: JSON output directory does not exist: %s", outputDir.string().c_str());
            return 1;
        }
    }

    std::string sharedLibraryPath = std::filesystem::current_path().string();

    int resultCode = 0;
    BenchReport report{};
    std::string resultsText;
    std::string resultsJson;
    try {
        LLM llm;

        std::unique_ptr<LlmBench> bench;
        if (scenarioMode) {
            // Scenario mode uses explicit prompt/image payloads.
            bench = std::make_unique<LlmBench>(llm, numInputTokens, numOutputTokens, scenario.value());
        } else {
            bench = std::make_unique<LlmBench>(llm, numInputTokens, numOutputTokens);
        }
        if (bench->Initialize(modelPath, numThreads, contextSize, sharedLibraryPath, modelRootPath) != 0) {
            LOG_ERROR("Benchmark initialization failed.");
            return 1;
        }

        BenchRunner runner(*bench, BenchRunConfig{numWarmup, numIterations});
        resultCode = runner.Run(report);
        if (resultCode == 0) {
            resultsText = BenchRunner::FormatText(report,
                                                  modelPath,
                                                  contextSize,
                                                  numThreads,
                                                  numInputTokens,
                                                  numOutputTokens,
                                                  bench->GetFrameworkType());
            resultsJson = BenchRunner::FormatJson(report,
                                                  modelPath,
                                                  contextSize,
                                                  numThreads,
                                                  numInputTokens,
                                                  numOutputTokens,
                                                  bench->GetFrameworkType());
        }
    } catch (const std::exception& ex) {
        LOG_ERROR("Benchmark execution failed: %s", ex.what());
        resultCode = 1;
    } catch (...) {
        LOG_ERROR("Benchmark execution failed: unknown error");
        resultCode = 1;
    }

    if (resultCode != 0 && resultsText.empty()) {
        resultsText = "No benchmark results available.\n";
    }
    std::cout << resultsText << std::endl;
    if (!jsonOutputPath.empty()) {
        if (resultCode != 0) {
            LOG_ERROR("JSON output requested but benchmark failed; no file written.");
            return resultCode;
        }
        std::ofstream out(jsonOutputPath);
        if (!out) {
            LOG_ERROR("Failed to open JSON output file: %s", jsonOutputPath.c_str());
            return 1;
        }
        out << resultsJson << std::endl;
        std::cout << "JSON output written to: " << jsonOutputPath << "\n";
    }
    return resultCode;
}
