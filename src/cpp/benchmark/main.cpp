//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmBenchmark.hpp"
#include <cstdlib>
#include <iostream>
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
              << " [--warmup <n>] [--help]\n\n";

    std::cerr << "Options:\n";
    std::cerr << "  --model,     -m    Path to LLM model config/file\n";
    std::cerr << "  --input,     -i    Number of input tokens for benchmark\n";
    std::cerr << "  --output,    -o    Number of output tokens to generate\n";
    std::cerr << "  --threads,   -t    Number of runtime threads\n";
    std::cerr << "  --iterations,-n    Number of benchmark iterations (default: 5)\n";
    std::cerr << "  --warmup,    -w    Number of warm-up iterations (default: 1)\n";
    std::cerr << "  --help,      -h    Show this help message and exit\n\n";

    std::cerr << "Example:\n";
    std::cerr << "  " << prog
              << " --model models/llama2.json"
              << " --input 128 --output 128"
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
    int numInputTokens   = 0;
    int numOutputTokens  = 0;
    int numThreads       = 0;
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
                std::cerr << "Missing value for argument: " << name << "\n";
                PrintUsage(argv[0]);
                std::exit(1);
            }
        };

        if (arg == "--model" || arg == "-m") {
            requireValue(arg);
            modelPath = argv[++i];
        }
        else if (arg == "--input" || arg == "-i") {
            requireValue(arg);
            numInputTokens = std::atoi(argv[++i]);
        }
        else if (arg == "--output" || arg == "-o") {
            requireValue(arg);
            numOutputTokens = std::atoi(argv[++i]);
        }
        else if (arg == "--threads" || arg == "-t") {
            requireValue(arg);
            numThreads = std::atoi(argv[++i]);
        }
        else if (arg == "--iterations" || arg == "-n") {
            requireValue(arg);
            numIterations = std::atoi(argv[++i]);
        }
        else if (arg == "--warmup" || arg == "-w") {
            requireValue(arg);
            numWarmup = std::atoi(argv[++i]);
        }
        else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }

    // Basic validation
    if (modelPath.empty() ||
        numInputTokens <= 0 ||
        numOutputTokens <= 0 ||
        numThreads <= 0 ||
        numIterations <= 0 ||
        numWarmup < 0) {

        std::cerr << "Error: Missing or invalid arguments.\n";
        PrintUsage(argv[0]);
        return 1;
    }

    std::string sharedLibraryPath = std::filesystem::current_path().string();
    // Run benchmark
    LlmBenchmark bench(modelPath,
                       numInputTokens,
                       numOutputTokens,
                       numThreads,
                       numIterations,
                       numWarmup,
                       sharedLibraryPath);

    bench.Run();
    auto results = bench.GetResults();
    std::cout << results << std::endl;
    return 1;
}
