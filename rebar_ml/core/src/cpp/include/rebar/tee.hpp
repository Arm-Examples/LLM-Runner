//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "rebar/rebar.hpp"

#include <cstddef>
#include <memory>

namespace rebar_ml {

struct TeeOptions {
    // Number of outputs per modality.
    std::size_t output_count = 2;
};

// A simple fan-out ("tee") node that exposes:
//
// - 1x input port per modality (text/audio/image)
// - N output ports per modality (as configured by TeeOptions::output_count)
//
// The node duplicates each received item to every output of the matching type.
std::shared_ptr<INode> make_tee_node(
    TeeOptions options = {});

}  // namespace rebar_ml
