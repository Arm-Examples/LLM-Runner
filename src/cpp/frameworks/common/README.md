<!--
    SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>

    SPDX-License-Identifier: Apache-2.0
-->

# Framework Common Utilities

This directory contains small utilities that are useful to more than one
framework backend but are not part of the public LLM API.

## TokenQueue

`TokenQueue.hpp` provides a thread-safe blocking queue for streamed token text.
It is intended for backend implementations that receive generated tokens from a
runtime callback on one thread and expose those tokens through the wrapper API on
another thread.

The queue owns the concurrency details:

- internal mutex and condition variable
- FIFO storage for pending token strings
- blocking wait/dequeue operations
- epoch-based invalidation of stale callbacks
- close/reset notification for waiting consumers

Keeping these operations inside `TokenQueue` avoids mixing backend encode or
generation logic with low-level synchronization code.

## Structure

Each generation has a queue epoch:

- `reset()` clears pending tokens, opens a new epoch, and returns that epoch ID.
- `resetAndClose()` clears pending tokens, invalidates stale producers, closes
  the new epoch, and wakes waiters. Use it for cancellation or idle initial
  state.
- Producer callbacks pass that epoch ID to `enqueue(epoch, token)`.
- `enqueue()` accepts a token only if the epoch is still current.
- `dequeue()` blocks until a token is available, the epoch changes, or the queue
  is closed.
- `close(epoch)` wakes waiters and marks that epoch as complete without accepting
  more tokens.

The epoch check prevents stale callbacks from a previous generation from adding
tokens after cancellation, reset, or a new encode request.

## Usage Pattern

Backend code should keep generation status and backend-specific errors outside
the queue, and use `TokenQueue` only for token delivery and wakeup semantics.

```cpp
uint64_t epoch = m_tokenQueue.reset();

auto tokenCallback = [this, epoch](const std::string& token) {
    if (!token.empty()) {
        m_tokenQueue.enqueue(epoch, token);
    }
};

// Start backend generation on its worker thread.

m_tokenQueue.waitForToken(epoch);

std::string token = m_tokenQueue.dequeue();
if (token.empty()) {
    // The generation ended, was cancelled, or the queue was reset.
}

m_tokenQueue.close(epoch);
```

Use `close(epoch)` when generation ends normally so waiters do not block
forever. Use `resetAndClose()` when cancelling or stopping a generation so
queued tokens are dropped, blocked consumers wake with an empty token, and stale
callback tokens are ignored. Use `reset()` before starting a new generation.
