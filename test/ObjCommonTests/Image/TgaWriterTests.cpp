#include "Image/Texture.h"
#include "Image/TgaWriter.h"

#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <vector>

namespace image::tga_writer
{
    namespace
    {
        constexpr auto HEADER_SIZE = 18uz;
        constexpr auto OFFSET_IMAGE_TYPE = 2uz;
        constexpr auto OFFSET_WIDTH = 12uz;
        constexpr auto OFFSET_HEIGHT = 14uz;
        constexpr auto OFFSET_PIXEL_DEPTH = 16uz;
        constexpr auto OFFSET_DESCRIPTOR = 17uz;

        std::string Dump(const Texture& texture)
        {
            std::ostringstream out;
            TgaWriter().DumpImage(out, &texture);

            return out.str();
        }
    } // namespace

    TEST_CASE("TgaWriter: Only supports the formats a tga file can hold as is", "[image]")
    {
        TgaWriter writer;

        CHECK(writer.SupportsImageFormat(&format::B8_G8_R8));
        CHECK(writer.SupportsImageFormat(&format::R8));
        CHECK(writer.SupportsImageFormat(&format::B8_G8_R8_A8));
        CHECK_FALSE(writer.SupportsImageFormat(&format::BC3));
    }

    TEST_CASE("TgaWriter: Writes true color pixels bottom row first", "[image]")
    {
        Texture2D texture(&format::B8_G8_R8, 2u, 2u);
        texture.Allocate();

        // Two rows of two pixels, already in the blue green red order of both the format and a tga file
        constexpr std::uint8_t pixels[]{
            1u,
            2u,
            3u,
            4u,
            5u,
            6u, // top row
            7u,
            8u,
            9u,
            10u,
            11u,
            12u, // bottom row
        };
        std::ranges::copy(pixels, texture.GetBufferForMipLevel(0, 0));

        const auto data = Dump(texture);
        REQUIRE(data.size() == HEADER_SIZE + sizeof(pixels));

        const auto* bytes = reinterpret_cast<const std::uint8_t*>(data.data());
        CHECK(bytes[OFFSET_IMAGE_TYPE] == 2u);
        CHECK(bytes[OFFSET_WIDTH] == 2u);
        CHECK(bytes[OFFSET_HEIGHT] == 2u);
        CHECK(bytes[OFFSET_PIXEL_DEPTH] == 24u);
        // Without the origin flag a tga file starts at the bottom row
        CHECK(bytes[OFFSET_DESCRIPTOR] == 0u);

        const std::vector expectedPixels(&bytes[HEADER_SIZE], &bytes[data.size()]);
        CHECK(expectedPixels == std::vector<std::uint8_t>{7u, 8u, 9u, 10u, 11u, 12u, 1u, 2u, 3u, 4u, 5u, 6u});
    }

    TEST_CASE("TgaWriter: Writes single channel textures as grayscale", "[image]")
    {
        Texture2D texture(&format::R8, 3u, 1u);
        texture.Allocate();

        constexpr std::uint8_t pixels[]{16u, 32u, 48u};
        std::ranges::copy(pixels, texture.GetBufferForMipLevel(0, 0));

        const auto data = Dump(texture);
        REQUIRE(data.size() == HEADER_SIZE + sizeof(pixels));

        const auto* bytes = reinterpret_cast<const std::uint8_t*>(data.data());
        CHECK(bytes[OFFSET_IMAGE_TYPE] == 3u);
        CHECK(bytes[OFFSET_WIDTH] == 3u);
        CHECK(bytes[OFFSET_HEIGHT] == 1u);
        CHECK(bytes[OFFSET_PIXEL_DEPTH] == 8u);

        const std::vector expectedPixels(&bytes[HEADER_SIZE], &bytes[data.size()]);
        CHECK(expectedPixels == std::vector<std::uint8_t>{16u, 32u, 48u});
    }
} // namespace image::tga_writer
