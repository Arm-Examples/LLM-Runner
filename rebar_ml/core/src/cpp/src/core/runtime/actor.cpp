//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "actor.hpp"

#include <cassert>
#include <condition_variable>

namespace rebar_ml::internal {

Actor::Queue::Queue(std::size_t capacity)
    : capacity_(capacity), buffer_(capacity) {

        if (capacity < 1) {
            capacity = 1;
        }
        this->capacity_ = capacity;
    }

bool Actor::Queue::push(Task task) {
    std::unique_lock lock(mutex_);
    not_full_.wait(lock, [&] { return stopped_ || size_ < capacity_; });
    if (stopped_) return false;
    buffer_[tail_] = std::move(task);
    tail_ = (tail_ + 1) % capacity_;
    ++size_;
    lock.unlock();
    not_empty_.notify_one();
    return true;
}

bool Actor::Queue::pop(Task& out) {
    std::unique_lock lock(mutex_);
    not_empty_.wait(lock, [&] { return stopped_ || size_ > 0; });
    if (size_ == 0 && stopped_) return false;
    out = std::move(*buffer_[head_]);
    buffer_[head_].reset();
    head_ = (head_ + 1) % capacity_;
    --size_;
    lock.unlock();
    not_full_.notify_one();
    return true;
}

void Actor::Queue::stop() {
    std::lock_guard lock(mutex_);
    stopped_ = true;
    not_empty_.notify_all();
    not_full_.notify_all();
}

Actor::Actor(std::size_t capacity, bool multi_enqueuer)
    : queue_(capacity),
      multi_enqueuer_(multi_enqueuer),
      thread_([this] { run(); }) {}

Actor::~Actor() {
    stop();
}

bool Actor::post(Task task) {
#ifndef NDEBUG
    if (!multi_enqueuer_) {
        if (enqueuer_thread_ == std::thread::id{}) {
            enqueuer_thread_ = std::this_thread::get_id();
        } else {
            assert(enqueuer_thread_ == std::this_thread::get_id() &&
                "Actor has multiple enqueuer threads");
        }
    }
#endif
    return queue_.push(std::move(task));
}

void Actor::stop() {
    if (stopped_.exchange(true)) return;
    queue_.stop();
    if (thread_.joinable()) thread_.join();
}

void Actor::wait_idle() {
    if (stopped_.load()) return;
    auto completed = std::make_shared<std::promise<void>>();
    auto completion = completed->get_future();
    if (!queue_.push([completed] { completed->set_value(); })) return;
    completion.wait();
}

void Actor::run() {
    Task task;
    while (queue_.pop(task)) {
        if (task) task();
        task = {};
    }
}

}  // namespace rebar_ml::internal
