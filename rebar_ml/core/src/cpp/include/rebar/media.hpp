//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "rebar/core.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace rebar_ml {

using Bytes = std::shared_ptr<const std::vector<std::byte>>;

enum class AudioCodec {
    pcm_s16le,
    wav,
    pcm_f32le,
};

struct AudioFormat {
    AudioCodec codec = AudioCodec::pcm_s16le;
    std::uint32_t sample_rate_hz = 0;
    std::uint16_t channels = 0;
};

struct AudioData {
    Bytes bytes;
    AudioFormat format;
    std::optional<std::string> language;
};

struct TextData {
    std::string text;
    std::optional<std::string> language;
};

struct ImageFormat {
    std::string mime_type;
    std::optional<std::uint32_t> width;
    std::optional<std::uint32_t> height;
};

struct ImageData {
    Bytes bytes;
    ImageFormat format;
};

template <>
constexpr TypeId type_id_of<TextData>() {
    return TypeId::text;
}

template <>
constexpr TypeId type_id_of<AudioData>() {
    return TypeId::audio;
}

template <>
constexpr TypeId type_id_of<ImageData>() {
    return TypeId::image;
}

using ITextInputPort = IInputPort<TextData>;
using IAudioInputPort = IInputPort<AudioData>;
using IImageInputPort = IInputPort<ImageData>;
using ITextOutputPort = IOutputPort<TextData>;
using IAudioOutputPort = IOutputPort<AudioData>;
using IImageOutputPort = IOutputPort<ImageData>;

using ITextInput = IInputStream<TextData>;
using IAudioInput = IInputStream<AudioData>;
using IImageInput = IInputStream<ImageData>;
using ITextOutput = IOutputStream<TextData>;
using IAudioOutput = IOutputStream<AudioData>;
using IImageOutput = IOutputStream<ImageData>;

}  // namespace rebar_ml
