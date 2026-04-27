//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//
#ifndef FRAMEWORKS_COMMON_TOKEN_QUEUE_HPP
#define FRAMEWORKS_COMMON_TOKEN_QUEUE_HPP

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <utility>

/**
 * @class TokenQueue
 * @brief Thread-safe blocking queue for streamed token text.
 *
 * The queue owns the synchronization used to pass generated tokens from backend
 * callbacks to the public LLM API. The epoch gates producers from older
 * generations so stale callbacks cannot enqueue tokens after a reset.
 *
 * Backend implementations should keep generation state and backend-specific
 * errors outside this class. TokenQueue is deliberately limited to token
 * delivery, wakeup, close, and reset semantics so encode/generation logic does
 * not need to own mutex/condition-variable coordination directly.
 */
class TokenQueue {
public:
    TokenQueue() = default;
    ~TokenQueue() = default;

    TokenQueue(const TokenQueue&) = delete;
    TokenQueue& operator=(const TokenQueue&) = delete;
    TokenQueue(TokenQueue&&) = delete;
    TokenQueue& operator=(TokenQueue&&) = delete;

    /**
     * @brief Enqueue a token if the provided epoch matches the current epoch.
     * @param epoch Generation epoch associated with the producer callback.
     * @param token Token text to enqueue. Moved into the queue when accepted.
     */
    void enqueue(uint64_t epoch, std::string token)
    {
        bool pushed = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (epoch == m_epoch && !m_closed) {
                m_queue.push_back(std::move(token));
                ++m_numQueued;
                pushed = true;
            }
        }

        if (pushed) {
            m_conditionVariable.notify_one();
        }
    }

    /**
     * @brief Dequeue a token, blocking until data is available or the queue is
     * reset/closed.
     * @return The next token, or an empty string when reset/closed.
     */
    std::string dequeue()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        const uint64_t seenEpoch = m_epoch;

        m_conditionVariable.wait(lock, [&] {
            return !m_queue.empty() || m_epoch != seenEpoch || m_closed;
        });

        if (m_queue.empty() || m_epoch != seenEpoch) {
            return std::string{};
        }

        std::string token = std::move(m_queue.front());
        m_queue.pop_front();
        return token;
    }

    /**
     * @brief Wait until this epoch has a token available, is reset, or is closed.
     * @param epoch Generation epoch to wait on.
     * @return true if a token is available for supplied or specified epoch
     */
    bool waitForToken(uint64_t epoch)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_conditionVariable.wait(lock, [&] {
            return !m_queue.empty() || m_epoch != epoch || m_closed;
        });
        return m_epoch == epoch && !m_queue.empty();
    }

    /**
     * @brief Bump the epoch, clear queued tokens, and notify waiters.
     * @return The new epoch opened by the reset.
     */
    uint64_t reset()
    {
        uint64_t newEpoch;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.clear();
            m_numQueued = 0;
            m_closed = false;
            newEpoch = ++m_epoch;
        }
        m_conditionVariable.notify_all();
        return newEpoch;
    }

    /**
     * @brief Bump the epoch, clear queued tokens, close it, and notify waiters.
     * @return The new closed epoch.
     */
    uint64_t resetAndClose()
    {
        uint64_t newEpoch;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.clear();
            m_numQueued = 0;
            m_closed = true;
            newEpoch = ++m_epoch;
        }
        m_conditionVariable.notify_all();
        return newEpoch;
    }

    /**
     * @brief Close the current epoch and wake waiters without accepting more tokens.
     * @param epoch Generation epoch to close. Ignored if it is no longer current.
     */
    void close(uint64_t epoch)
    {
        bool notify = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (epoch == m_epoch) {
                m_closed = true;
                notify = true;
            }
        }

        if (notify) {
            m_conditionVariable.notify_all();
        }
    }

    /**
     * @brief Check whether the current epoch has no queued tokens.
     * @return true if no tokens are queued, false otherwise.
     */
    bool empty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    /**
     * @brief Get the number of tokens queued since the last reset.
     * @return Number of tokens accepted into the current epoch.
     */
    uint64_t numQueued() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_numQueued;
    }

    /**
     * @brief Get the current queue epoch.
     * @return Current epoch value.
     */
    uint64_t epoch() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_epoch;
    }

private:
    mutable std::mutex m_mutex{};
    std::condition_variable m_conditionVariable{};
    std::deque<std::string> m_queue{};
    uint64_t m_epoch{0};
    uint64_t m_numQueued{0};
    bool m_closed{false};
};

#endif /* FRAMEWORKS_COMMON_TOKEN_QUEUE_HPP */
