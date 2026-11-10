//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

namespace rebar_ml::internal {

class Actor final {
public:
    using Task = std::function<void()>;

    Actor(std::size_t capacity, bool multi_enqueuer);
    ~Actor();

    bool post(Task task);
    void stop();
    void wait_idle();

private:
    class Queue {
    public:
        explicit Queue(std::size_t capacity);

        bool push(Task task);
        bool pop(Task& out);
        void stop();

    private:
        std::mutex mutex_;
        std::condition_variable not_empty_;
        std::condition_variable not_full_;
        std::size_t capacity_ = 0;
        std::vector<std::optional<Task>> buffer_;
        std::size_t head_ = 0;
        std::size_t tail_ = 0;
        std::size_t size_ = 0;
        bool stopped_ = false;
    };

    void run();

    Queue queue_;
    [[maybe_unused]] bool multi_enqueuer_ = true;
    std::thread thread_;
    std::atomic<bool> stopped_{false};

#ifndef NDEBUG
    std::thread::id enqueuer_thread_;
#endif
};

}  // namespace rebar_ml::internal
