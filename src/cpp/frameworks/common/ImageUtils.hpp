//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#ifndef LLM_FRAMEWORKS_COMMON_IMAGE_UTILS_HPP
#define LLM_FRAMEWORKS_COMMON_IMAGE_UTILS_HPP

#include <string>

namespace ImageUtils {

/**
 * @struct ImageSize
 * @brief Pixel dimensions for an image.
 */
struct ImageSize {
    /** Image width in pixels. */
    int width{0};
    /** Image height in pixels. */
    int height{0};
};

/**
 * @struct PreparedImage
 * @brief Result of preparing an image file for framework input.
 */
struct PreparedImage {
    /** Path to the prepared image file. */
    std::string path;
    /** Pixel dimensions of the prepared image. */
    ImageSize size;
};

/**
 * @brief Read image dimensions from disk without fully decoding image pixels.
 * @param imagePath Path to the image file.
 * @return Image width and height in pixels.
 */
ImageSize ReadImageSize(const std::string& imagePath);

/**
 * @brief Compute aspect-ratio preserving resize dimensions.
 * @param original Original image dimensions in pixels.
 * @param maxInputDimension Maximum allowed width or height in pixels.
 * @return Original dimensions when already within the limit; otherwise resized dimensions.
 */
ImageSize ComputeResizedImageSize(const ImageSize& original, int maxInputDimension);

/**
 * @brief Resize an image and write it to disk as a PNG.
 * @param inputPath Path to the source image file.
 * @param outputPath Path where the resized PNG should be written.
 * @param maxInputDimension Maximum allowed width or height in pixels.
 * @return Prepared image path and dimensions.
 */
PreparedImage ResizeImageToFile(const std::string& inputPath,
                                const std::string& outputPath,
                                int maxInputDimension);

} // namespace ImageUtils

#endif // LLM_FRAMEWORKS_COMMON_IMAGE_UTILS_HPP
