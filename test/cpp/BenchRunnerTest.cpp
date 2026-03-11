//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "BenchRunner.hpp"
#include "LlmBench.hpp"

#include <atomic>
#include <cmath>
#include <nlohmann/json.hpp>
#include <vector>

#include "catch2/catch_approx.hpp"
#include "catch2/catch_test_macros.hpp"

namespace {

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

    mutable int buildCalls = 0;
    int encodeCalls = 0;
    int decodeCalls = 0;
    int stopCalls = 0;
    int finishCalls = 0;

private:
    int m_outputTokens;
};

} // namespace

TEST_CASE("BenchRunner: rejects non-positive measured iterations")
{
    FakeBench bench(4);
    BenchRunner runner(bench, BenchRunConfig{1, 0});
    BenchReport report{};

    const int rc = runner.Run(report);

    CHECK(rc == 1);
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

    const int rc = runner.Run(report);

    REQUIRE(rc == 0);
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
    BenchReport report{};
    report.config = BenchRunConfig{1, 2};
    report.results = {
        BenchIterationResult{11.1119, 22.2229, 4, 1.2349, 2.3459, 100.1239, 50.5679},
    };
    report.summary = BenchRunner::ComputeSummaryStats(report.results);

    const std::string output = BenchRunner::FormatJson(report, "/tmp/model.bin", 512, 2, 128, 64, "mnn");
    const auto parsed = nlohmann::json::parse(output);

    CHECK(parsed["parameters"]["model_path"] == "/tmp/model.bin");
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
}

TEST_CASE("BenchRunner: text formatter includes key headers and parameters")
{
    BenchReport report{};
    report.config = BenchRunConfig{0, 1};
    report.results = {
        BenchIterationResult{10.0, 20.0, 4, 1.0, 2.0, 100.0, 2.0},
    };
    report.summary = BenchRunner::ComputeSummaryStats(report.results);

    const std::string text = BenchRunner::FormatText(report, "/tmp/model.bin", 256, 4, 128, 64, "mnn");

    CHECK(text.find("ARM LLM Benchmark") != std::string::npos);
    CHECK(text.find("model_path         : /tmp/model.bin") != std::string::npos);
    CHECK(text.find("Framework") != std::string::npos);
    CHECK(text.find("Threads") != std::string::npos);
    CHECK(text.find("Performance") != std::string::npos);
    CHECK(text.find("TTFT") != std::string::npos);
}

TEST_CASE("LlmBench: BuildIterationResult computes throughput rules")
{
    const BenchEncodeStepResult encode{2.0};
    const BenchDecodeStepResult decode{10, 5.0, 100.0};

    const auto generic = LlmBench::BuildIterationResult(encode, decode, 200, "mnn");
    CHECK(generic.timeToFirstTokenMs == Catch::Approx(2100.0));
    CHECK(generic.totalTimeMs == Catch::Approx(7000.0));
    CHECK(generic.encodeTokensPerSec == Catch::Approx(100.0));
    CHECK(generic.decodeTokensPerSec == Catch::Approx(2.0));

    const auto mediapipe = LlmBench::BuildIterationResult(encode, decode, 200, "mediapipe");
    CHECK(mediapipe.encodeTokensPerSec == Catch::Approx(95.238).epsilon(0.001));
}

TEST_CASE("LlmBench: BuildIterationResult handles zero durations")
{
    const BenchEncodeStepResult encode{0.0};
    const BenchDecodeStepResult decode{0, 0.0, 0.0};

    const auto out = LlmBench::BuildIterationResult(encode, decode, 128, "mnn");

    CHECK(out.encodeTokensPerSec == Catch::Approx(0.0));
    CHECK(out.decodeTokensPerSec == Catch::Approx(0.0));
    CHECK(out.totalTimeMs == Catch::Approx(0.0));
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
