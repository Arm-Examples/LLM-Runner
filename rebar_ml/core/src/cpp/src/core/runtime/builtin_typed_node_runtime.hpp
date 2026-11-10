//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "rebar/media.hpp"

#include "node_impl.hpp"

namespace rebar_ml::internal {

class BuiltInTypedNodeRuntime : public NodeRuntime {
public:
    ~BuiltInTypedNodeRuntime() override = default;

    Result<void> on_payload(PortId port, AnyPayload payload) override {
        if (payload.type_id() == type_id_of<TextData>()) {
            return on_text(port, take_payload<TextData>(std::move(payload)));
        }
        if (payload.type_id() == type_id_of<AudioData>()) {
            return on_audio(port, take_payload<AudioData>(std::move(payload)));
        }
        if (payload.type_id() == type_id_of<ImageData>()) {
            return on_image(port, take_payload<ImageData>(std::move(payload)));
        }
        return unexpected(Error{ErrorCode::port_not_available, "input type is unavailable"});
    }

    virtual Result<void> on_text(std::size_t, TextData) {
        return unexpected(Error{ErrorCode::port_not_available, "text input is unavailable"});
    }

    virtual Result<void> on_audio(std::size_t, AudioData) {
        return unexpected(Error{ErrorCode::port_not_available, "audio input is unavailable"});
    }

    virtual Result<void> on_image(std::size_t, ImageData) {
        return unexpected(Error{ErrorCode::port_not_available, "image input is unavailable"});
    }
};

}  // namespace rebar_ml::internal
