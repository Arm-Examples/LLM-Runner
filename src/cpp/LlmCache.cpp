//
// SPDX-FileCopyrightText: Copyright 2025-2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "LlmCache.hpp"


LlmHandle LLMCache::Add(std::unique_ptr<LLM> obj) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const LlmHandle handle = m_nextHandle++;
    m_cache.emplace(handle, std::move(obj));
    return handle;
}

LLM* LLMCache::Lookup(LlmHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(handle);
    return (it == m_cache.end()) ? nullptr : it->second.get();
}

void LLMCache::Remove(LlmHandle handle) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.erase(handle); // unique_ptr frees object
}
