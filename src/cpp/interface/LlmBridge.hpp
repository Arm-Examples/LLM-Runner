//
// SPDX-FileCopyrightText: Copyright 2025 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <atomic>

/**
 * @brief Result codes for operations.
 *
 * Used to indicate success, cancellation, or an error condition.
 */
enum ResultCode {
    RESULT_OK = 0,        ///< Operation completed successfully.
    RESULT_CANCELLED = 1, ///< Operation was cancelled.
    RESULT_ERROR = 2      ///< Operation failed with an error.
};

/**
 * @brief State associated with a unit of work.
 *
 * Holds the identifier used to reference a work item and a flag indicating
 * whether cancellation has been requested.
 */
struct WorkState {
    long operationId;           ///< Unique identifier for this work item.
    std::atomic<bool> cancelled;///< True if cancellation was requested.
};

// Internal utilities (defined in .cpp)

/**
 * @brief Register or update a work item in the registry.
 *
 * Inserts @p state keyed by its @c operationId. Replaces any existing entry
 * with the same identifier.
 *
 * @param state Shared pointer to the work state to add or update.
 */
void addWork(const std::shared_ptr<WorkState>& state);

/**
 * @brief Look up a work item by its operation ID.
 *
 * @param operationId Identifier of the work item to find.
 * @return A shared pointer to the corresponding state if found; otherwise @c nullptr.
 */
std::shared_ptr<WorkState> findWork(long operationId);

/**
 * @brief Remove a work item by its operation ID.
 *
 * Erases the entry for @p operationId, if present, and returns the removed state.
 *
 * @param operationId Identifier of the work item to remove.
 * @return The removed state if it existed; otherwise @c nullptr.
 */
std::shared_ptr<WorkState> removeWork(long operationId);
