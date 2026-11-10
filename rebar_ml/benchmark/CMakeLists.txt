#
# SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
#
# SPDX-License-Identifier: Apache-2.0
#

# Placeholder for future Rebar pipeline benchmarks.
#
# Intended scope:
# - end-to-end pipeline latency/throughput harnesses
# - per-node timing hooks (where available)
# - quick "benchmark smoke" runs that can be wired into CTest (not perf-gating)
#
# This directory is intentionally minimal until the benchmark API/CLI is added.

add_library(rebar_ml_benchmark_interface INTERFACE)
