//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "BenchRunner.hpp"
#include "LlmBench.hpp"

#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <vector>

#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

namespace {

std::filesystem::path CreateTinyModelFile()
{
    const auto path = std::filesystem::temp_directory_path() / "bench-runner-test-model.bin";
    std::ofstream out(path, std::ios::binary);
    out << 'x';
    return path;
}

std::filesystem::path CreateEmptyModelFile()
{
    const auto path = std::filesystem::temp_directory_path() / "bench-runner-empty-model.bin";
    std::ofstream out(path, std::ios::binary);
    return path;
}

std::filesystem::path CreateEmptyModelDir()
{
    const auto path = std::filesystem::temp_directory_path() / "bench-runner-empty-model-dir";
    std::filesystem::remove_all(path);
    std::filesystem::create_directory(path);
    return path;
}

class FakeBench final : public IBenchAdapter {
public:
    explicit FakeBench(int outputTokens)
        : m_outputTokens(outputTokens)
    {}

    BenchEncodeStepResult EncodeStep() override
    {
        ++encodeCalls;
        return BenchEncodeStepResult{0.010};
    }

    BenchDecodeStepResult DecodeStep() override
    {
        ++decodeCalls;
        return BenchDecodeStepResult{1, 0.002, 1.5};
    }

    BenchIterationResult BuildIterationResult(const BenchEncodeStepResult& encodeResult,
                                              const BenchDecodeStepResult& decodeResult) const override
    {
        ++buildCalls;
        BenchIterationResult result{};
        result.encodeTimeSec = encodeResult.encodeTimeSec;
        result.decodeTimeSec = decodeResult.decodeTimeSec;
        result.tokensGenerated = decodeResult.tokensGenerated;
        result.timeToFirstTokenMs = (encodeResult.encodeTimeSec * 1000.0) + decodeResult.firstTokenFromDecodeStartMs;
        result.totalTimeMs = (encodeResult.encodeTimeSec + decodeResult.decodeTimeSec) * 1000.0;
        if (encodeResult.encodeTimeSec > 0.0) {
            result.encodeTokensPerSec = 100.0;
        }
        if (decodeResult.decodeTimeSec > 0.0) {
            result.decodeTokensPerSec = static_cast<double>(decodeResult.tokensGenerated) / decodeResult.decodeTimeSec;
        }
        return result;
    }

    void StopGeneration() override { ++stopCalls; }
    void FinishIteration() override { ++finishCalls; }
    int GetOutputTokens() const override { return m_outputTokens; }
    uintmax_t GetModelSizeBytes() const override { return modelSizeBytes; }

    mutable int buildCalls = 0;
    int encodeCalls = 0;
    int decodeCalls = 0;
    int stopCalls = 0;
    int finishCalls = 0;
    uintmax_t modelSizeBytes = 1;

private:
    int m_outputTokens;
};

} // namespace

TEST_CASE("BenchRunner: rejects non-positive measured iterations")
{
    FakeBench bench(4);
    BenchRunner runner(bench, BenchRunConfig{1, 0});
    BenchReport report{};

    const int resultCode = runner.Run(report);

    CHECK(resultCode == 1);
    CHECK(bench.encodeCalls == 0);
    CHECK(bench.decodeCalls == 0);
    CHECK(bench.stopCalls == 0);
    CHECK(bench.finishCalls == 0);
}

TEST_CASE("BenchRunner: warmup is excluded and measured iterations are recorded")
{
    FakeBench bench(4);
    BenchRunner runner(bench, BenchRunConfig{2, 3});
    BenchReport report{};

    const int resultCode = runner.Run(report);

    REQUIRE(resultCode == 0);
    CHECK(report.results.size() == 3);
    CHECK(report.config.warmupIterations == 2);
    CHECK(report.config.measuredIterations == 3);

    CHECK(bench.encodeCalls == 5);
    CHECK(bench.decodeCalls == 20);
    CHECK(bench.stopCalls == 5);
    CHECK(bench.finishCalls == 5);
    CHECK(bench.buildCalls == 5);

    for (const auto& r : report.results) {
        CHECK(r.tokensGenerated == 4);
        CHECK(r.timeToFirstTokenMs == Catch::Approx(11.5).margin(0.5));
    }
}

TEST_CASE("BenchRunner: computes summary statistics correctly")
{
    const std::vector<BenchIterationResult> results{
        BenchIterationResult{10.0, 20.0, 4, 1.0, 2.0, 100.0, 2.0},
        BenchIterationResult{20.0, 40.0, 6, 2.0, 3.0, 120.0, 3.0},
    };

    const auto stats = BenchRunner::ComputeSummaryStats(results);

    CHECK(stats.mean.timeToFirstTokenMs == Catch::Approx(15.0));
    CHECK(stats.mean.totalTimeMs == Catch::Approx(30.0));
    CHECK(stats.mean.encodeTokensPerSec == Catch::Approx(110.0));
    CHECK(stats.mean.decodeTokensPerSec == Catch::Approx(2.5));

    CHECK(stats.stddev.timeToFirstTokenMs == Catch::Approx(5.0));
    CHECK(stats.stddev.totalTimeMs == Catch::Approx(10.0));
    CHECK(stats.stddev.encodeTokensPerSec == Catch::Approx(10.0));
    CHECK(stats.stddev.decodeTokensPerSec == Catch::Approx(0.5));
}

TEST_CASE("BenchRunner: JSON formatter emits expected schema and rounded values")
{
    const auto modelPath = CreateTinyModelFile();
    BenchReport report{};
    report.config = BenchRunConfig{1, 2};
    report.modelSizeBytes = std::filesystem::file_size(modelPath);
    report.results = {
        BenchIterationResult{11.1119, 22.2229, 4, 1.2349, 2.3459, 100.1239, 50.5679},
    };
    report.summary = BenchRunner::ComputeSummaryStats(report.results);

    const std::string output = BenchRunner::FormatJson(report, modelPath.string(), 512, 2, 128, 64, "mnn");
    const auto parsed = nlohmann::json::parse(output);

    CHECK(parsed["parameters"]["model_path"] == modelPath.string());
    CHECK(parsed["parameters"]["model_size"] == "0.00 GB");
    CHECK(parsed["parameters"]["num_input_tokens"] == 128);
    CHECK(parsed["parameters"]["num_output_tokens"] == 64);
    CHECK(parsed["parameters"]["context_size"] == 512);
    CHECK(parsed["parameters"]["num_threads"] == 2);
    CHECK(parsed["parameters"]["num_iterations"] == 2);
    CHECK(parsed["parameters"]["num_warmup"] == 1);
    CHECK(parsed["framework"] == "mnn");
    CHECK(parsed["iterations"].size() == 1);
    CHECK(parsed["iterations"][0]["time_to_first_token_ms"] == 11.112);
    CHECK(parsed["iterations"][0]["total_time_ms"] == 22.223);

    std::filesystem::remove(modelPath);
}

TEST_CASE("BenchRunner: text formatter includes key headers and parameters")
{
    const auto modelPath = CreateTinyModelFile();
    BenchReport report{};
    report.config = BenchRunConfig{0, 1};
    report.modelSizeBytes = std::filesystem::file_size(modelPath);
    report.results = {
        BenchIterationResult{10.0, 20.0, 4, 1.0, 2.0, 100.0, 2.0},
    };
    report.summary = BenchRunner::ComputeSummaryStats(report.results);

    const std::string text = BenchRunner::FormatText(report, modelPath.string(), 256, 4, 128, 64, "mnn");

    CHECK(text.find("ARM LLM Benchmark") != std::string::npos);
    CHECK(text.find("model_path         : " + modelPath.string()) != std::string::npos);
    CHECK(text.find("model_size         : 0.00 GB") != std::string::npos);
    CHECK(text.find("Framework") != std::string::npos);
    CHECK(text.find("Threads") != std::string::npos);
    CHECK(text.find("Performance") != std::string::npos);
    CHECK(text.find("TTFT") != std::string::npos);

    std::filesystem::remove(modelPath);
}

TEST_CASE("LlmBench: BuildIterationResult computes throughput rules")
{
    const BenchEncodeStepResult encode{2.0};
    const BenchDecodeStepResult decode{10, 5.0, 100.0};

    const auto generic = LlmBench::BuildIterationResult(encode, decode, 200);
    CHECK(generic.timeToFirstTokenMs == Catch::Approx(2100.0));
    CHECK(generic.totalTimeMs == Catch::Approx(7000.0));
    CHECK(generic.encodeTokensPerSec == Catch::Approx(100.0));
    CHECK(generic.decodeTokensPerSec == Catch::Approx(2.0));
}

TEST_CASE("LlmBench: BuildIterationResult handles zero durations")
{
    const BenchEncodeStepResult encode{0.0};
    const BenchDecodeStepResult decode{0, 0.0, 0.0};

    const auto out = LlmBench::BuildIterationResult(encode, decode, 128);

    CHECK(out.encodeTokensPerSec == Catch::Approx(0.0));
    CHECK(out.decodeTokensPerSec == Catch::Approx(0.0));
    CHECK(out.totalTimeMs == Catch::Approx(0.0));
}

TEST_CASE("LlmBench: Initialize rejects empty model file")
{
    const auto modelPath = CreateEmptyModelFile();
    LLM llm;
    LlmBench bench(llm, 1, 1);

    const int resultCode = bench.Initialize(modelPath.string(), 1, 4, ".");

    CHECK(resultCode == 1);
    std::filesystem::remove(modelPath);
}

TEST_CASE("LlmBench: Initialize rejects empty model directory")
{
    const auto modelPath = CreateEmptyModelDir();
    LLM llm;
    LlmBench bench(llm, 1, 1);

    const int resultCode = bench.Initialize(modelPath.string(), 1, 4, ".");

    CHECK(resultCode == 1);
    std::filesystem::remove_all(modelPath);
}

TEST_CASE("LlmBench: MeasureTimingSec executes callable and returns elapsed time")
{
    std::atomic<int> callCount{0};
    const double elapsed = LlmBench::MeasureTimingSec("bench.test", [&]() {
        ++callCount;
    });

    CHECK(callCount.load() == 1);
    CHECK(elapsed >= 0.0);
}

TEST_CASE("LlmBench: Initialize rejects invalid benchmark settings")
{
    const auto modelPath = CreateTinyModelFile();

    SECTION("input tokens must be positive") {
        LLM llm;
        LlmBench bench(llm, 0, 1);
        CHECK(bench.Initialize(modelPath.string(), 1, 4, ".") == 1);
    }

    SECTION("output tokens must be positive") {
        LLM llm;
        LlmBench bench(llm, 1, 0);
        CHECK(bench.Initialize(modelPath.string(), 1, 4, ".") == 1);
    }

    SECTION("threads must be positive") {
        LLM llm;
        LlmBench bench(llm, 1, 1);
        CHECK(bench.Initialize(modelPath.string(), 0, 4, ".") == 1);
    }

    SECTION("context size must be positive") {
        LLM llm;
        LlmBench bench(llm, 1, 1);
        CHECK(bench.Initialize(modelPath.string(), 1, 0, ".") == 1);
    }

    SECTION("model path must not be empty") {
        LLM llm;
        LlmBench bench(llm, 1, 1);
        CHECK(bench.Initialize("", 1, 4, ".") == 1);
    }

    std::filesystem::remove(modelPath);
}

TEST_CASE("LlmBench: Initialize rejects context size that cannot fit requested tokens")
{
    const auto modelPath = CreateTinyModelFile();
    LLM llm;
    LlmBench bench(llm, 3, 2);

    CHECK(bench.Initialize(modelPath.string(), 1, 5, ".") == 1);
    CHECK(bench.Initialize(modelPath.string(), 1, 4, ".") == 1);

    std::filesystem::remove(modelPath);
}

TEST_CASE("LlmBench: Initialize rejects nonexistent model path")
{
    const auto modelPath = std::filesystem::temp_directory_path() / "bench-runner-missing-model.bin";
    std::filesystem::remove(modelPath);

    LLM llm;
    LlmBench bench(llm, 1, 1);

    CHECK(bench.Initialize(modelPath.string(), 1, 4, ".") == 1);
}

TEST_CASE("LlmBench: BuildIterationResult handles zero encode time")
{
    const BenchEncodeStepResult encode{0.0};
    const BenchDecodeStepResult decode{5, 2.0, 50.0};

    const auto result = LlmBench::BuildIterationResult(encode, decode, 100);

    CHECK(result.encodeTokensPerSec == 0.0);
    CHECK(result.decodeTokensPerSec == Catch::Approx(2.5));
    CHECK(result.totalTimeMs == Catch::Approx(2000.0));
}

TEST_CASE("LlmBench: BuildIterationResult handles zero decode time")
{
    const BenchEncodeStepResult encode{1.0};
    const BenchDecodeStepResult decode{0, 0.0, 0.0};

    const auto result = LlmBench::BuildIterationResult(encode, decode, 50);

    CHECK(result.encodeTokensPerSec == Catch::Approx(50.0));
    CHECK(result.decodeTokensPerSec == 0.0);
    CHECK(result.totalTimeMs == Catch::Approx(1000.0));
}

TEST_CASE("LlmBench: GetOutputTokens and GetInputTokens return configured values")
{
    LLM llm;
    LlmBench bench(llm, 128, 64);

    CHECK(bench.GetInputTokens() == 128);
    CHECK(bench.GetOutputTokens() == 64);
}

TEST_CASE("BenchRunner: single iteration produces zero stdev")
{
    const std::vector<BenchIterationResult> singleResult{
        BenchIterationResult{10.0, 20.0, 4, 1.0, 2.0, 100.0, 2.0},
    };

    const auto stats = BenchRunner::ComputeSummaryStats(singleResult);

    // With n=1, mean should equal the single value
    CHECK(stats.mean.timeToFirstTokenMs == Catch::Approx(10.0));
    CHECK(stats.mean.totalTimeMs == Catch::Approx(20.0));

    // stddev should be 0 (no variance with single sample)
    CHECK(stats.stddev.timeToFirstTokenMs == Catch::Approx(0.0).margin(1e-9));
    CHECK(stats.stddev.totalTimeMs == Catch::Approx(0.0).margin(1e-9));
}

TEST_CASE("BenchRunner: empty results produce zero statistics")
{
    const std::vector<BenchIterationResult> emptyResults{};

    const auto stats = BenchRunner::ComputeSummaryStats(emptyResults);

    CHECK(stats.mean.timeToFirstTokenMs == 0.0);
    CHECK(stats.mean.totalTimeMs == 0.0);
    CHECK(stats.stddev.timeToFirstTokenMs == 0.0);
    CHECK(stats.stddev.totalTimeMs == 0.0);
}

TEST_CASE("BenchRunner: FormatText and FormatJson output can be parsed independently")
{
    const auto modelPath = CreateTinyModelFile();
    BenchReport report{};
    report.config = BenchRunConfig{1, 2};
    report.modelSizeBytes = std::filesystem::file_size(modelPath);
    report.results = {
        BenchIterationResult{11.1119, 22.2229, 4, 1.2349, 2.3459, 100.1239, 50.5679},
        BenchIterationResult{10.5, 21.0, 4, 1.0, 2.0, 100.0, 50.0},
    };
    report.summary = BenchRunner::ComputeSummaryStats(report.results);

    const std::string textOutput = BenchRunner::FormatText(report, modelPath.string(), 512, 2, 128, 64, "llama.cpp");
    const std::string jsonOutput = BenchRunner::FormatJson(report, modelPath.string(), 512, 2, 128, 64, "llama.cpp");

    // Text should at least contain framework name
    CHECK(textOutput.find("llama.cpp") != std::string::npos);

    // JSON should parse and contain iteration count matching report
    const auto parsed = nlohmann::json::parse(jsonOutput);
    CHECK(parsed["iterations"].size() == 2);
    CHECK(parsed["framework"] == "llama.cpp");

    std::filesystem::remove(modelPath);
}
