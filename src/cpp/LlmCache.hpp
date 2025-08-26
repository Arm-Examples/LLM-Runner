//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//


#include <unordered_map>
#include <memory>
#include <mutex>
#include <jni.h>
#include "LlmImpl.hpp"

#pragma once

/**
 * @class LLMCache
 * @brief Thread-safe singleton cache mapping opaque handles to LLM instances.
 *
 * Stores ownership of LLM objects and returns an opaque 64-bit handle (jlong)
 * for lookup and removal. Public member functions are thread-safe via a mutex.
 */
class LLMCache {
public:

    /**
     * Access the global singleton instance (thread-safe API).
     * @return singleton instance of LLMCache.
     */
    static LLMCache& Instance() {
        static LLMCache inst;
        return inst;
    }
    
    /**
     * Insert an LLM object and obtain its opaque handle.
     * @param obj LLM instance to be added to the cache.
     * @return The new handle associated with the LLM instance.
     */
    jlong Add(std::unique_ptr<LLM> obj);

    /**
     * Look up an LLM by its handle (nullptr if not found).
     * @param handle handle associated with LLM instance.
     * @return LLM instance, null if not found
     */
    LLM* Lookup(jlong handle);

    /**
     * Remove LLM associated with the handle.
     * @param handle handle associated with LLM instance.
     */ 
    void Remove(jlong handle);

private:
    LLMCache() = default;
    ~LLMCache() = default; 

    LLMCache(const LLMCache&) = delete;
    LLMCache& operator=(const LLMCache&) = delete;

    std::mutex m_mutex;
    std::unordered_map<jlong, std::unique_ptr<LLM>> m_cache;
    jlong m_nextHandle = 1;
};