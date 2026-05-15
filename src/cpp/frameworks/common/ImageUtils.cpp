//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "ImageUtils.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "Logger.hpp"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace ImageUtils {

ImageSize ReadImageSize(const std::string& imagePath)
{
    if (imagePath.empty()) {
        THROW_INVALID_ARGUMENT("Image path must not be empty");
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info(imagePath.c_str(), &width, &height, &channels) == 0) {
        const char* reason = stbi_failure_reason();
        THROW_INVALID_ARGUMENT("Failed to read image size for '%s': %s",
                               imagePath.c_str(),
                               reason ? reason : "unknown error");
    }

    if (width <= 0 || height <= 0) {
        THROW_INVALID_ARGUMENT("Invalid image size for '%s': width=%d height=%d",
                               imagePath.c_str(),
                               width,
                               height);
    }

    return ImageSize{width, height};
}

ImageSize ComputeResizedImageSize(const ImageSize& original, int maxInputDimension)
{
    if (original.width <= 0 || original.height <= 0) {
        THROW_INVALID_ARGUMENT("Invalid original image size: width=%d height=%d",
                               original.width,
                               original.height);
    }
    if (maxInputDimension <= 0) {
        THROW_INVALID_ARGUMENT("Max input dimension must be > 0");
    }

    const int longestSide = std::max(original.width, original.height);
    if (longestSide <= maxInputDimension) {
        return original;
    }

    const double scale = static_cast<double>(maxInputDimension) / static_cast<double>(longestSide);
    const int resizedWidth = std::max(1, static_cast<int>(std::lround(original.width * scale)));
    const int resizedHeight = std::max(1, static_cast<int>(std::lround(original.height * scale)));

    return ImageSize{
        std::min(resizedWidth, maxInputDimension),
        std::min(resizedHeight, maxInputDimension)
    };
}

PreparedImage ResizeImageToFile(const std::string& inputPath,
                                const std::string& outputPath,
                                int maxInputDimension)
{
    if (outputPath.empty()) {
        THROW_INVALID_ARGUMENT("Output image path must not be empty");
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* inputPixels = stbi_load(inputPath.c_str(), &width, &height, &channels, 3);
    if (!inputPixels) {
        const char* reason = stbi_failure_reason();
        THROW_INVALID_ARGUMENT("Failed to load image '%s': %s",
                               inputPath.c_str(),
                               reason ? reason : "unknown error");
    }

    const ImageSize originalSize{width, height};
    const ImageSize resizedSize = ComputeResizedImageSize(originalSize, maxInputDimension);

    std::vector<unsigned char> outputPixels(static_cast<size_t>(resizedSize.width) *
                                            static_cast<size_t>(resizedSize.height) * 3);

    unsigned char* resizedPixels = stbir_resize_uint8_srgb(inputPixels,
                                                           width,
                                                           height,
                                                           0,
                                                           outputPixels.data(),
                                                           resizedSize.width,
                                                           resizedSize.height,
                                                           0,
                                                           STBIR_RGB);
    stbi_image_free(inputPixels);

    if (!resizedPixels) {
        THROW_ERROR("Failed to resize image '%s' to width=%d height=%d",
                    inputPath.c_str(),
                    resizedSize.width,
                    resizedSize.height);
    }

    if (stbi_write_png(outputPath.c_str(),
                       resizedSize.width,
                       resizedSize.height,
                       3,
                       outputPixels.data(),
                       resizedSize.width * 3) == 0) {
        THROW_ERROR("Failed to write resized image to '%s'", outputPath.c_str());
    }

    return PreparedImage{outputPath, resizedSize};
}

} // namespace ImageUtils
