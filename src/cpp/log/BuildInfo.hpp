//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef ARM_LLM_WRAPPER_BUILD_INFO_HPP
#define ARM_LLM_WRAPPER_BUILD_INFO_HPP

#include <string>

namespace LlmLog {

/**
 * @struct BuildMetadata
 * @brief Immutable build and dependency metadata embedded at CMake configure time.
 *
 * This structure exposes the module identity, build timestamp, selected backend,
 * and pinned backend dependency revisions so they can be emitted consistently in
 * CLI logs and Android logcat.
 */
struct BuildMetadata {
    const char* moduleName;
    const char* moduleVersion;
    const char* moduleGitSha;
    const char* buildTimestampUtc;
    const char* frameworkName;
    const char* frameworkDependencyVersions;
};

/**
 * @brief Return the configured build metadata singleton.
 * @return Reference to the process-wide build metadata record.
 */
const BuildMetadata& GetBuildMetadata();

/**
 * @brief Format build metadata as a single log-friendly line.
 * @return Human-readable metadata string for CLI/logcat output.
 */
std::string FormatBuildMetadata();

/**
 * @brief Emit the formatted build metadata once per process.
 *
 * Repeated calls are intentionally deduplicated so every application linked
 * against the library reports the metadata at initialization without spamming
 * logs on subsequent LLM creations.
 */
void LogBuildMetadataOnce();

/**
 * @brief Emit a standardized initialization failure log entry.
 * @param frameworkName Framework/backend name to report. Falls back to the configured build metadata if empty.
 * @param errorMessage Failure description to append to the log message.
 */
void LogInitializationFailure(const std::string& frameworkName, const std::string& errorMessage);

}  // namespace LlmLog

#endif  // ARM_LLM_WRAPPER_BUILD_INFO_HPP
