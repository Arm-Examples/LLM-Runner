//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmCache.hpp"
#include "catch2/catch_test_macros.hpp"
#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

template <typename Function>
void RunConcurrently(const int numThreads, Function&& func)
{
    std::atomic<int> readyThreads{0};
    std::atomic<bool> start{false};

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&] {
            readyThreads.fetch_add(1);

            while (!start.load()) {
                std::this_thread::yield();
            }

            func();
        });
    }

    while (readyThreads.load() != numThreads) {
        std::this_thread::yield();
    }

    start.store(true);

    for (auto& thread : threads) {
        thread.join();
    }
}

TEST_CASE("LLMCache: add, lookup, and remove LLM instance")
{
    auto llm = std::make_unique<LLM>();
    LLM* rawPtr = llm.get();

    const LlmHandle handle = LLMCache::Instance().Add(std::move(llm));

    REQUIRE(handle > 0);
    CHECK(LLMCache::Instance().Lookup(handle) == rawPtr);

    LLMCache::Instance().Remove(handle);

    CHECK(LLMCache::Instance().Lookup(handle) == nullptr);
}

TEST_CASE("LLMCache: lookup of missing handles returns null")
{
    CHECK(LLMCache::Instance().Lookup(0) == nullptr);
    CHECK(LLMCache::Instance().Lookup(-1) == nullptr);
    CHECK(LLMCache::Instance().Lookup(999999999L) == nullptr);
}

TEST_CASE("LLMCache: removing missing or already removed handles is harmless")
{
    constexpr LlmHandle missingHandle = 888888888L;

    CHECK_NOTHROW(LLMCache::Instance().Remove(missingHandle));

    auto llm = std::make_unique<LLM>();
    const LlmHandle handle = LLMCache::Instance().Add(std::move(llm));

    LLMCache::Instance().Remove(handle);

    CHECK_NOTHROW(LLMCache::Instance().Remove(handle));
    CHECK(LLMCache::Instance().Lookup(handle) == nullptr);
}

TEST_CASE("LLMCache: added handles are distinct")
{
    auto first = std::make_unique<LLM>();
    auto second = std::make_unique<LLM>();

    const LlmHandle firstHandle = LLMCache::Instance().Add(std::move(first));
    const LlmHandle secondHandle = LLMCache::Instance().Add(std::move(second));

    CHECK(firstHandle > 0);
    CHECK(secondHandle > 0);
    CHECK(firstHandle != secondHandle);

    LLMCache::Instance().Remove(firstHandle);
    LLMCache::Instance().Remove(secondHandle);
}

TEST_CASE("LLMCache: concurrent adds produce unique lookupable handles")
{
    constexpr int numThreads = 8;
    constexpr int itemsPerThread = 100;

    std::vector<LlmHandle> allHandles;
    std::mutex resultMutex;
    std::atomic<bool> success{true};

    RunConcurrently(numThreads, [&] {
        for (int i = 0; i < itemsPerThread; ++i) {
            auto llm = std::make_unique<LLM>();
            const LlmHandle handle = LLMCache::Instance().Add(std::move(llm));

            if (handle <= 0) {
                success.store(false);
            }

            {
                std::lock_guard<std::mutex> lock(resultMutex);
                allHandles.push_back(handle);
            }
        }
    });

    REQUIRE(success.load());
    REQUIRE(allHandles.size() == numThreads * itemsPerThread);

    std::sort(allHandles.begin(), allHandles.end());

    CHECK(std::adjacent_find(allHandles.begin(), allHandles.end()) == allHandles.end());

    for (const auto handle : allHandles) {
        CHECK(LLMCache::Instance().Lookup(handle) != nullptr);
    }

    for (const auto handle : allHandles) {
        LLMCache::Instance().Remove(handle);
    }
}

TEST_CASE("LLMCache: concurrent add lookup remove cycles are safe")
{
    constexpr int numThreads = 8;
    constexpr int iterations = 100;

    std::atomic<bool> success{true};

    RunConcurrently(numThreads, [&] {
        for (int i = 0; i < iterations; ++i) {
            auto llm = std::make_unique<LLM>();
            const LlmHandle handle = LLMCache::Instance().Add(std::move(llm));

            if (handle <= 0) {
                success.store(false);
            }

            if (LLMCache::Instance().Lookup(handle) == nullptr) {
                success.store(false);
            }

            LLMCache::Instance().Remove(handle);

            if (LLMCache::Instance().Lookup(handle) != nullptr) {
                success.store(false);
            }
        }
    });

    CHECK(success.load());
}

TEST_CASE("LLMCache: concurrent lookups of existing handles are safe")
{
    constexpr int numHandles = 20;
    constexpr int numThreads = 8;
    constexpr int lookupsPerThread = 1000;

    std::vector<LlmHandle> handles;
    handles.reserve(numHandles);

    for (int i = 0; i < numHandles; ++i) {
        auto llm = std::make_unique<LLM>();
        handles.push_back(LLMCache::Instance().Add(std::move(llm)));
    }

    std::atomic<bool> success{true};

    RunConcurrently(numThreads, [&] {
        for (int i = 0; i < lookupsPerThread; ++i) {
            for (const auto handle : handles) {
                if (LLMCache::Instance().Lookup(handle) == nullptr) {
                    success.store(false);
                }
            }
        }
    });

    CHECK(success.load());

    for (const auto handle : handles) {
        LLMCache::Instance().Remove(handle);
    }
}

TEST_CASE("LLMCache: concurrent remove of same handle is harmless")
{
    constexpr int numThreads = 8;
    constexpr int removesPerThread = 100;

    auto llm = std::make_unique<LLM>();
    const LlmHandle handle = LLMCache::Instance().Add(std::move(llm));

    REQUIRE(LLMCache::Instance().Lookup(handle) != nullptr);

    RunConcurrently(numThreads, [&] {
        for (int i = 0; i < removesPerThread; ++i) {
            LLMCache::Instance().Remove(handle);
        }
    });

    CHECK(LLMCache::Instance().Lookup(handle) == nullptr);
}

TEST_CASE("LLMCache: concurrent lookup of missing handles is safe")
{
    constexpr int numThreads = 8;
    constexpr int lookupsPerThread = 1000;

    std::atomic<bool> success{true};

    RunConcurrently(numThreads, [&] {
        for (int i = 0; i < lookupsPerThread; ++i) {
            if (LLMCache::Instance().Lookup(0) != nullptr) {
                success.store(false);
            }

            if (LLMCache::Instance().Lookup(-1) != nullptr) {
                success.store(false);
            }

            if (LLMCache::Instance().Lookup(999999999L) != nullptr) {
                success.store(false);
            }
        }
    });

    CHECK(success.load());
}