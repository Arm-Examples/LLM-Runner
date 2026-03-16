//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "BuildInfo.hpp"

#include <mutex>
#include <sstream>

#include "Logger.hpp"
#include "LlmBuildInfoConfig.hpp"

namespace {

/**
 * Build metadata compiled into the library from the generated CMake header.
 */
const LlmLog::BuildMetadata kBuildMetadata{
    ARM_LLM_BUILD_MODULE_NAME,
    ARM_LLM_BUILD_MODULE_VERSION,
    ARM_LLM_BUILD_MODULE_GIT_SHA,
    ARM_LLM_BUILD_TIMESTAMP_UTC,
    ARM_LLM_BUILD_FRAMEWORK_NAME,
    ARM_LLM_BUILD_FRAMEWORK_DEPENDENCY_VERSIONS,
};

/**
 * Resolve the framework name to log, preferring the runtime-supplied backend
 * but falling back to the configured framework if no explicit value is given.
 */
std::string ResolveFrameworkName(const std::string& frameworkName) {
    if (!frameworkName.empty()) {
        return frameworkName;
    }
    return kBuildMetadata.frameworkName;
}

}  // namespace

namespace LlmLog {

/**
 * Return the immutable build metadata record for the current binary.
 */
const BuildMetadata& GetBuildMetadata()
{
    return kBuildMetadata;
}

/**
 * Assemble the build metadata into a compact single-line representation.
 */
std::string FormatBuildMetadata()
{
    std::ostringstream oss;
    oss << kBuildMetadata.moduleName
        << " version=" << kBuildMetadata.moduleVersion;

    if (std::string(kBuildMetadata.moduleGitSha) != "" &&
        std::string(kBuildMetadata.moduleGitSha) != "unknown") {
        oss << " git_sha=" << kBuildMetadata.moduleGitSha;
    }

    oss << " build_timestamp_utc=" << kBuildMetadata.buildTimestampUtc
        << " framework=" << kBuildMetadata.frameworkName;

    if (std::string(kBuildMetadata.frameworkDependencyVersions) != "" &&
        std::string(kBuildMetadata.frameworkDependencyVersions) != "unknown") {
        oss << " framework_revisions=[" << kBuildMetadata.frameworkDependencyVersions << "]";
    }

    return oss.str();
}

/**
 * Log the build metadata exactly once for the lifetime of the process.
 */
void LogBuildMetadataOnce()
{
    static std::once_flag once;
    std::call_once(once, []() {
        LOG_BUILD_INFO("%s", FormatBuildMetadata().c_str());
    });
}

/**
 * Log a standardized initialization failure including the framework/backend name.
 */
void LogInitializationFailure(const std::string& frameworkName, const std::string& errorMessage)
{
    LOG_ERROR("LLM initialization failed using framework='%s': %s",
              ResolveFrameworkName(frameworkName).c_str(),
              errorMessage.c_str());
}

}  // namespace LlmLog
