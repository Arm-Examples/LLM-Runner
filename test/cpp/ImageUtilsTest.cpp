//
// SPDX-FileCopyrightText: Copyright 2026 Arm Limited and/or its affiliates <open-source-office@arm.com>
//
// SPDX-License-Identifier: Apache-2.0
//

#include "ImageUtils.hpp"

#include <filesystem>
#include <stdexcept>
#include <string>

#include "catch2/catch_test_macros.hpp"

TEST_CASE("ImageUtils computes aspect-ratio preserving resize dimensions")
{
    SECTION("Landscape image scales longest side to max dimension") {
        const auto resized = ImageUtils::ComputeResizedImageSize(ImageUtils::ImageSize{1024, 768}, 128);
        CHECK(resized.width == 128);
        CHECK(resized.height == 96);
    }

    SECTION("Portrait image scales longest side to max dimension") {
        const auto resized = ImageUtils::ComputeResizedImageSize(ImageUtils::ImageSize{768, 1024}, 128);
        CHECK(resized.width == 96);
        CHECK(resized.height == 128);
    }

    SECTION("Small image is not upscaled") {
        const auto resized = ImageUtils::ComputeResizedImageSize(ImageUtils::ImageSize{64, 48}, 128);
        CHECK(resized.width == 64);
        CHECK(resized.height == 48);
    }

    SECTION("Invalid max dimension is rejected") {
        CHECK_THROWS_AS(ImageUtils::ComputeResizedImageSize(ImageUtils::ImageSize{64, 48}, 0), std::invalid_argument);
    }
}

TEST_CASE("ImageUtils reads image dimensions")
{
    const std::string imagePath = std::string{TEST_RESOURCE_DIR} + "/cat.bmp";
    const auto size = ImageUtils::ReadImageSize(imagePath);

    CHECK(size.width > 0);
    CHECK(size.height > 0);
}

TEST_CASE("ImageUtils saves resized image")
{
    const std::string imagePath = std::string{TEST_RESOURCE_DIR} + "/cat.bmp";
    const auto outputPath = std::filesystem::temp_directory_path() / "llm-wrapper-image-utils-test.png";

    std::filesystem::remove(outputPath);
    const auto prepared = ImageUtils::ResizeImageToFile(imagePath, outputPath.string(), 128);

    CHECK(prepared.path == outputPath.string());
    CHECK(prepared.size.width > 0);
    CHECK(prepared.size.height > 0);
    CHECK(prepared.size.width <= 128);
    CHECK(prepared.size.height <= 128);
    CHECK(std::filesystem::exists(outputPath));

    const auto savedSize = ImageUtils::ReadImageSize(outputPath.string());
    CHECK(savedSize.width == prepared.size.width);
    CHECK(savedSize.height == prepared.size.height);

    std::filesystem::remove(outputPath);
}
