//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "BenchScenario.hpp"
#include "BenchRunner.hpp"
#include "LlmBench.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

namespace {

std::filesystem::path CreateUniqueTemporaryPath(const std::string &prefix)
{
    static std::atomic<std::uint64_t> sequence{0};
    static const std::uint64_t processSalt = [] {
        const auto timestamp = static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());
        std::random_device randomDevice;
        return timestamp ^ (static_cast<std::uint64_t>(randomDevice()) << 32U) ^ randomDevice();
    }();

    return std::filesystem::temp_directory_path() /
           (prefix + "-" + std::to_string(processSalt) + "-" + std::to_string(sequence.fetch_add(1)));
}

std::filesystem::path CreateTinyModelFile()
{
    const auto path = CreateUniqueTemporaryPath("bench-runner-test-model.bin");
    std::ofstream out(path, std::ios::binary);
    out << 'x';
    return path;
}

std::filesystem::path CreateEmptyModelFile()
{
    const auto path = CreateUniqueTemporaryPath("bench-runner-empty-model.bin");
    std::ofstream out(path, std::ios::binary);
    return path;
}

std::filesystem::path CreateEmptyModelDir()
{
    const auto path = CreateUniqueTemporaryPath("bench-runner-empty-model-dir");
    std::filesystem::remove_all(path);
    std::filesystem::create_directory(path);
    return path;
}

std::filesystem::path CreateScenarioDir()
{
    const auto path = CreateUniqueTemporaryPath("bench-scenario-test");
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

std::filesystem::path CreateVisionConfigDir()
{
    const auto path = CreateUniqueTemporaryPath("bench-vision-config-test");
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

std::filesystem::path WriteTextFile(const std::filesystem::path& path, const std::string& contents)
{
    std::ofstream out(path, std::ios::binary);
    out << contents;
    return path;
}

std::string BuildModelConfigJson(bool isVision)
{
    nlohmann::json config;
    config["chat"] = {
        {"systemPrompt", ""},
        {"applyDefaultChatTemplate", true},
        {"systemTemplate", "%s"},
        {"userTemplate", "%s"},
    };
    config["model"] = {
        {"llmModelName", "backend/model.bin"},
        {"projModelName", "backend/projector.bin"},
        {"isVision", isVision},
    };
    config["runtime"] = {
        {"batchSize", 1},
        {"numThreads", 1},
        {"contextSize", 16},
    };
    config["stopWords"] = {"endoftext"};
    return config.dump(2);
}

BenchScenario MakeImageScenario()
{
    BenchScenario scenario{};
    scenario.imagePath = "image.bmp";
    return scenario;
}

class ScopedCurrentPath {
public:
    explicit ScopedCurrentPath(const std::filesystem::path& path)
        : m_original(std::filesystem::current_path())
    {
        std::filesystem::current_path(path);
    }

    ~ScopedCurrentPath()
    {
        std::filesystem::current_path(m_original);
    }

    ScopedCurrentPath(const ScopedCurrentPath&) = delete;
    ScopedCurrentPath& operator=(const ScopedCurrentPath&) = delete;

private:
    std::filesystem::path m_original;
};

std::filesystem::path WriteScenarioFile(const std::filesystem::path& dir,
                                        const std::string& name,
                                        const std::string& jsonText)
{
    const auto path = dir / name;
    std::ofstream out(path);
    out << jsonText;
    return path;
}

BenchIterationResult MakeIterationResult(const double ttftMs,
                                         const double totalMs,
                                         const int generatedTokens,
                                         const double encodeSec,
                                         const double decodeSec,
                                         const std::optional<int> inputTokens,
                                         const std::optional<double> encodeTps,
                                         const double decodeTps,
                                         const std::optional<std::uint64_t> preparedImagePixels = std::nullopt,
                                         const std::optional<double> preparedImageMegapixelsPerEncodeSec = std::nullopt)
{
    BenchIterationResult result{};
    result.timeToFirstTokenMs = ttftMs;
    result.totalTimeMs = totalMs;
    result.tokensGenerated = generatedTokens;
    result.encodeTimeSec = encodeSec;
    result.decodeTimeSec = decodeSec;
    result.inputTokens = inputTokens;
    result.encodeTokensPerSec = encodeTps;
    result.decodeTokensPerSec = decodeTps;
    result.imageMetadata.preparedImagePixels = preparedImagePixels;
    result.preparedImageMegapixelsPerEncodeSec = preparedImageMegapixelsPerEncodeSec;
    return result;
}

class FakeBench final : public IBenchAdapter {
public:
    explicit FakeBench(const int outputTokens)
        : m_outputTokens(outputTokens)
    {}

    BenchEncodeStepResult EncodeStep() override
    {
        ++encodeCalls;
        if (updateWorkloadDuringEncode) {
            workload.isScenario = true;
            workload.workloadType = "image_text";
            workload.imageCount = 1;
            workload.imageMetadata.originalImagePixels = 12000;
            workload.imageMetadata.originalImageWidth = 150;
            workload.imageMetadata.originalImageHeight = 80;
            workload.imageMetadata.preparedImagePixels = 9216;
            workload.imageMetadata.preparedImageWidth = 128;
            workload.imageMetadata.preparedImageHeight = 72;
            workload.inputTokens.reset();
        }
        BenchEncodeStepResult result{};
        result.encodeTimeSec = 0.010;
        result.inferenceStats.originalImagePixels = workload.imageMetadata.originalImagePixels;
        result.inferenceStats.originalImageWidth = workload.imageMetadata.originalImageWidth;
        result.inferenceStats.originalImageHeight = workload.imageMetadata.originalImageHeight;
        result.inferenceStats.preparedImagePixels = workload.imageMetadata.preparedImagePixels;
        result.inferenceStats.preparedImageWidth = workload.imageMetadata.preparedImageWidth;
        result.inferenceStats.preparedImageHeight = workload.imageMetadata.preparedImageHeight;
        result.inferenceStats.imageCount = workload.imageCount;
        return result;
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
            result.inputTokens = 1;
            result.encodeTokensPerSec = 100.0;
        }
        if (decodeResult.decodeTimeSec > 0.0) {
            result.decodeTokensPerSec = static_cast<double>(decodeResult.tokensGenerated) / decodeResult.decodeTimeSec;
        }
        result.imageMetadata.originalImagePixels = encodeResult.inferenceStats.originalImagePixels;
        result.imageMetadata.originalImageWidth = encodeResult.inferenceStats.originalImageWidth;
        result.imageMetadata.originalImageHeight = encodeResult.inferenceStats.originalImageHeight;
        result.imageMetadata.preparedImagePixels = encodeResult.inferenceStats.preparedImagePixels;
        result.imageMetadata.preparedImageWidth = encodeResult.inferenceStats.preparedImageWidth;
        result.imageMetadata.preparedImageHeight = encodeResult.inferenceStats.preparedImageHeight;
        if (result.imageMetadata.preparedImagePixels.has_value() && encodeResult.encodeTimeSec > 0.0) {
            result.preparedImageMegapixelsPerEncodeSec =
                (static_cast<double>(result.imageMetadata.preparedImagePixels.value()) / 1000000.0) /
                encodeResult.encodeTimeSec;
        }
        return result;
    }

    void StopGeneration() override { ++stopCalls; }
    void FinishIteration() override { ++finishCalls; }
    int GetOutputTokens() const override { return m_outputTokens; }
    uintmax_t GetModelSizeBytes() const override { return modelSizeBytes; }
    BenchWorkloadMetadata GetWorkloadMetadata() const override { return workload; }

    mutable int buildCalls = 0;
    int encodeCalls = 0;
    int decodeCalls = 0;
    int stopCalls = 0;
    int finishCalls = 0;
    uintmax_t modelSizeBytes = 1;
    bool updateWorkloadDuringEncode = false;
    BenchWorkloadMetadata workload{};

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

TEST_CASE("BenchRunner: report captures workload metadata updated during encode")
{
    FakeBench bench(1);
    bench.updateWorkloadDuringEncode = true;
    BenchRunner runner(bench, BenchRunConfig{0, 1});
    BenchReport report{};

    const int resultCode = runner.Run(report);

    REQUIRE(resultCode == 0);
    CHECK(report.workload.isScenario);
    CHECK(report.workload.workloadType == "image_text");
    CHECK(report.workload.imageCount == 1);
    CHECK(report.workload.imageMetadata.preparedImageWidth == 128);
    CHECK(report.workload.imageMetadata.preparedImageHeight == 72);
    REQUIRE(report.workload.imageMetadata.originalImagePixels.has_value());
    CHECK(report.workload.imageMetadata.originalImagePixels.value() == 12000);
    REQUIRE(report.workload.imageMetadata.originalImageWidth.has_value());
    CHECK(report.workload.imageMetadata.originalImageWidth.value() == 150);
    REQUIRE(report.workload.imageMetadata.originalImageHeight.has_value());
    CHECK(report.workload.imageMetadata.originalImageHeight.value() == 80);
    REQUIRE(report.workload.imageMetadata.preparedImagePixels.has_value());
    CHECK(report.workload.imageMetadata.preparedImagePixels.value() == 9216);
    CHECK_FALSE(report.workload.inputTokens.has_value());
}

TEST_CASE("BenchRunner: computes summary statistics correctly")
{
    const std::vector<BenchIterationResult> results{
        MakeIterationResult(10.0, 20.0, 4, 1.0, 2.0, 100, 100.0, 2.0, 1000000, 1.0),
        MakeIterationResult(20.0, 40.0, 6, 2.0, 3.0, 240, 120.0, 3.0, 6000000, 3.0),
    };

    const auto stats = BenchRunner::ComputeSummaryStats(results);

    CHECK(stats.mean.timeToFirstTokenMs == Catch::Approx(15.0));
    CHECK(stats.mean.totalTimeMs == Catch::Approx(30.0));
    REQUIRE(stats.mean.encodeTokensPerSec.has_value());
    CHECK(stats.mean.encodeTokensPerSec.value() == Catch::Approx(110.0));
    CHECK(stats.mean.decodeTokensPerSec == Catch::Approx(2.5));
    REQUIRE(stats.mean.preparedImageMegapixelsPerEncodeSec.has_value());
    CHECK(stats.mean.preparedImageMegapixelsPerEncodeSec.value() == Catch::Approx(2.0));

    CHECK(stats.stddev.timeToFirstTokenMs == Catch::Approx(5.0));
    CHECK(stats.stddev.totalTimeMs == Catch::Approx(10.0));
    REQUIRE(stats.stddev.encodeTokensPerSec.has_value());
    CHECK(stats.stddev.encodeTokensPerSec.value() == Catch::Approx(10.0));
    CHECK(stats.stddev.decodeTokensPerSec == Catch::Approx(0.5));
    REQUIRE(stats.stddev.preparedImageMegapixelsPerEncodeSec.has_value());
    CHECK(stats.stddev.preparedImageMegapixelsPerEncodeSec.value() == Catch::Approx(1.0));
}

TEST_CASE("BenchRunner: JSON formatter emits expected schema and rounded values")
{
    const auto modelPath = CreateTinyModelFile();
    BenchReport report{};
    report.config = BenchRunConfig{1, 2};
    report.modelSizeBytes = std::filesystem::file_size(modelPath);
    report.workload.inputTokens = 128;
    report.results = {
        MakeIterationResult(11.1119, 22.2229, 4, 1.2349, 2.3459, 128, 100.1239, 50.5679),
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

TEST_CASE("LlmBench: image scenario requires JSON model config")
{
    const auto modelPath = CreateTinyModelFile();
    LLM llm;
    LlmBench bench(llm, 0, 1, MakeImageScenario());

    const int resultCode = bench.Initialize(modelPath.string(), 1, 4, ".");

    CHECK(resultCode == 1);
    std::filesystem::remove(modelPath);
}

TEST_CASE("LlmBench: image scenario rejects relative config model paths without model root")
{
    const auto dir = CreateVisionConfigDir();
    const auto configPath = WriteTextFile(dir / "vision-config.json", BuildModelConfigJson(true));
    LLM llm;
    LlmBench bench(llm, 0, 1, MakeImageScenario());

    const int resultCode = bench.Initialize(configPath.string(), 1, 4, ".");

    CHECK(resultCode == 1);
    std::filesystem::remove_all(dir);
}

TEST_CASE("LlmBench: image scenario requires vision config")
{
    const auto dir = CreateVisionConfigDir();
    const auto modelRoot = dir / "models";
    std::filesystem::create_directories(modelRoot);
    const auto configPath = WriteTextFile(dir / "text-config.json", BuildModelConfigJson(false));
    LLM llm;
    LlmBench bench(llm, 0, 1, MakeImageScenario());

    const int resultCode = bench.Initialize(configPath.string(), 1, 4, ".", modelRoot.string());

    CHECK(resultCode == 1);
    std::filesystem::remove_all(dir);
}

TEST_CASE("BenchScenario: parses image and prompt scenario")
{
    const auto dir = CreateScenarioDir();
    const auto imagePath = dir / "cat.bmp";
    std::filesystem::copy_file(std::string{TEST_RESOURCE_DIR} + "/cat.bmp",
                               imagePath,
                               std::filesystem::copy_options::overwrite_existing);
    const auto scenarioPath = WriteScenarioFile(dir,
                                                "scenario.json",
                                                R"JSON({
                                                    "image": "cat.bmp",
                                                    "prompt": "Describe the image",
                                                    "max_output_tokens": 7
                                                })JSON");

    const BenchScenario scenario = LoadBenchScenarioFromJson(scenarioPath.string());

    CHECK(scenario.HasImage());
    CHECK(scenario.HasPrompt());
    CHECK(scenario.ImageCount() == 1);
    CHECK(scenario.WorkloadType() == "image_text");
    CHECK(scenario.imagePath == imagePath.string());
    CHECK(scenario.maxOutputTokens == 7);

    std::filesystem::remove_all(dir);
}

TEST_CASE("BenchScenario: parses image-only scenario using image_path")
{
    const auto dir = CreateScenarioDir();
    const auto imagePath = dir / "cat.bmp";
    std::filesystem::copy_file(std::string{TEST_RESOURCE_DIR} + "/cat.bmp",
                               imagePath,
                               std::filesystem::copy_options::overwrite_existing);
    const auto scenarioPath = WriteScenarioFile(dir, "scenario.json", R"JSON({"image_path": "cat.bmp"})JSON");

    const BenchScenario scenario = LoadBenchScenarioFromJson(scenarioPath.string());

    CHECK(scenario.HasImage());
    CHECK_FALSE(scenario.HasPrompt());
    CHECK(scenario.WorkloadType() == "image");
    CHECK(scenario.imagePath == imagePath.string());

    std::filesystem::remove_all(dir);
}

TEST_CASE("BenchScenario: rejects empty scenario")
{
    const auto dir = CreateScenarioDir();
    const auto scenarioPath = WriteScenarioFile(dir, "scenario.json", "{}");

    try {
        (void)LoadBenchScenarioFromJson(scenarioPath.string());
        FAIL("Expected empty scenario to throw");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()).find("at least one") != std::string::npos);
    }

    std::filesystem::remove_all(dir);
}

TEST_CASE("BenchScenario: rejects missing image")
{
    const auto dir = CreateScenarioDir();
    const auto scenarioPath = WriteScenarioFile(dir, "scenario.json", R"JSON({"image": "missing.bmp"})JSON");

    try {
        (void)LoadBenchScenarioFromJson(scenarioPath.string());
        FAIL("Expected missing image to throw");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()).find("image file does not exist") != std::string::npos);
    }

    std::filesystem::remove_all(dir);
}

TEST_CASE("BenchScenario: resolves relative image path from scenario directory")
{
    const auto dir = CreateScenarioDir();
    std::filesystem::create_directories(dir / "images");
    const auto imagePath = dir / "images" / "cat.bmp";
    std::filesystem::copy_file(std::string{TEST_RESOURCE_DIR} + "/cat.bmp",
                               imagePath,
                               std::filesystem::copy_options::overwrite_existing);
    const auto scenarioPath = WriteScenarioFile(dir, "scenario.json", R"JSON({"image": "images/cat.bmp"})JSON");

    const BenchScenario scenario = LoadBenchScenarioFromJson(scenarioPath.string());

    CHECK(scenario.imagePath == imagePath.string());

    std::filesystem::remove_all(dir);
}

TEST_CASE("BenchScenario: preserves relative image path when scenario path is relative")
{
    const auto dir = CreateScenarioDir();
    std::filesystem::create_directories(dir / "scenarios");
    std::filesystem::create_directories(dir / "images");
    std::filesystem::copy_file(std::string{TEST_RESOURCE_DIR} + "/cat.bmp",
                               dir / "images" / "cat.bmp",
                               std::filesystem::copy_options::overwrite_existing);
    WriteScenarioFile(dir / "scenarios", "scenario.json", R"JSON({"image": "../images/cat.bmp"})JSON");

    {
        const ScopedCurrentPath currentPath(dir);
        const BenchScenario scenario = LoadBenchScenarioFromJson("scenarios/scenario.json");
        const std::filesystem::path expectedImagePath = std::filesystem::path("images") / "cat.bmp";
        CHECK(scenario.imagePath == expectedImagePath.string());
    }

    std::filesystem::remove_all(dir);
}

TEST_CASE("BenchRunner: formatter reports text scenario token metadata")
{
    BenchReport report{};
    report.config = BenchRunConfig{0, 1};
    report.modelSizeBytes = 1;
    report.workload.isScenario = true;
    report.workload.workloadType = "text";
    report.workload.imageCount = 0;
    report.workload.tokenMetadata.textPromptTokens = 42;
    report.workload.tokenMetadata.fusedPromptPositions = 42;
    report.workload.inputTokens.reset();

    auto iteration = MakeIterationResult(10.0, 20.0, 4, 0.005, 0.015,
                                         std::nullopt, std::nullopt, 266.6);
    iteration.tokenMetadata.textPromptTokens = 42;
    iteration.tokenMetadata.fusedPromptPositions = 42;
    iteration.prefillTimeMs = 5.0;
    report.results = {iteration};
    report.summary = BenchRunner::ComputeSummaryStats(report.results);

    const std::string text = BenchRunner::FormatText(report, "text-model", 256, 4, 0, 4, "executorch");
    CHECK(text.find("workload_type      : text") != std::string::npos);
    CHECK(text.find("image_count        : 0") != std::string::npos);
    CHECK(text.find("text_prompt_tokens : 42") != std::string::npos);
    CHECK(text.find("fused_positions") == std::string::npos);
    CHECK(text.find("vision_tokens") == std::string::npos);
    CHECK(text.find("prepared_image_size") == std::string::npos);

    const auto parsed = nlohmann::json::parse(
        BenchRunner::FormatJson(report, "text-model", 256, 4, 0, 4, "executorch"));
    CHECK(parsed["parameters"]["workload_type"] == "text");
    CHECK(parsed["parameters"]["image_count"] == 0);
    CHECK(parsed["parameters"]["text_prompt_tokens"] == 42);
    CHECK_FALSE(parsed["parameters"].contains("fused_prompt_positions"));
    CHECK_FALSE(parsed["parameters"].contains("vision_tokens"));
    CHECK_FALSE(parsed["parameters"].contains("prepared_image_width"));
}

TEST_CASE("BenchRunner: text formatter includes key headers and parameters")
{
    const auto modelPath = CreateTinyModelFile();
    BenchReport report{};
    report.config = BenchRunConfig{0, 1};
    report.modelSizeBytes = std::filesystem::file_size(modelPath);
    report.workload.inputTokens = 128;
    report.results = {
        MakeIterationResult(10.0, 20.0, 4, 1.0, 2.0, 128, 100.0, 2.0),
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

TEST_CASE("LlmBench: BuildIterationResult handles zero durations")
{
    const BenchEncodeStepResult encode{0.0};
    constexpr BenchDecodeStepResult decode{0, 0.0, 0.0};

    const auto out = LlmBench::BuildIterationResult(encode, decode, 128);

    CHECK(out.inputTokens == 128);
    CHECK_FALSE(out.encodeTokensPerSec.has_value());
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
    const double elapsed = LlmBench::MeasureTimingSec("bench.test", [&] {
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

    CHECK_FALSE(result.encodeTokensPerSec.has_value());
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
