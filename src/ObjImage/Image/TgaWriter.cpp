#include "TgaWriter.h"

#include <cassert>
#include <cstdint>
#include <limits>

using namespace image;

namespace
{
    constexpr auto TGA_HEADER_SIZE = 18u;
    constexpr uint8_t TGA_IMAGE_TYPE_TRUE_COLOR = 2u;
    constexpr uint8_t TGA_IMAGE_TYPE_GRAYSCALE = 3u;

    uint8_t GetImageType(const ImageFormat* format)
    {
        switch (format->GetId())
        {
        case ImageFormatId::B8_G8_R8:
        case ImageFormatId::B8_G8_R8_A8:
            return TGA_IMAGE_TYPE_TRUE_COLOR;
        case ImageFormatId::R8:
            return TGA_IMAGE_TYPE_GRAYSCALE;
        default:
            return 0u;
        }
    }

    void WriteUInt16(uint8_t* buffer, const unsigned value)
    {
        buffer[0] = static_cast<uint8_t>(value & 0xFFu);
        buffer[1] = static_cast<uint8_t>((value >> 8u) & 0xFFu);
    }
} // namespace

namespace image
{
    bool TgaWriter::SupportsImageFormat(const ImageFormat* imageFormat)
    {
        return GetImageType(imageFormat) != 0u;
    }

    std::string TgaWriter::GetFileExtension()
    {
        return ".tga";
    }

    void TgaWriter::DumpImage(std::ostream& stream, const Texture* texture)
    {
        const auto* format = texture->GetFormat();
        const auto imageType = GetImageType(format);
        assert(imageType != 0u);
        if (imageType == 0u)
            return;

        // Every supported format is unsigned and stores its channels in the order and size a tga file does, so the
        // pixel data goes out unchanged
        const auto* unsignedFormat = dynamic_cast<const ImageFormatUnsigned*>(format);

        const auto width = texture->GetWidth();
        const auto height = texture->GetHeight();
        assert(width <= std::numeric_limits<uint16_t>::max() && height <= std::numeric_limits<uint16_t>::max());

        uint8_t header[TGA_HEADER_SIZE]{};
        header[2] = imageType;
        WriteUInt16(&header[12], width);
        WriteUInt16(&header[14], height);
        header[16] = static_cast<uint8_t>(unsignedFormat->m_bits_per_pixel);
        // The low nibble of the image descriptor is how many bits of each pixel are alpha. A reader that does not
        // find it there treats a 32 bit image as opaque.
        header[17] = static_cast<uint8_t>(unsignedFormat->m_a_size & 0xFu);

        stream.write(reinterpret_cast<const char*>(header), sizeof(header));

        // A tga file without the origin flag set starts at the bottom row
        const auto pitch = format->GetPitch(0, width);
        const auto* firstRow = texture->GetBufferForMipLevel(0);
        for (auto row = height; row > 0u; row--)
            stream.write(reinterpret_cast<const char*>(&firstRow[(row - 1u) * pitch]), static_cast<std::streamsize>(pitch));
    }
} // namespace image
