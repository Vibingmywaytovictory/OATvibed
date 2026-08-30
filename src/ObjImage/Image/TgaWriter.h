#pragma once

#include "ImageWriter.h"

namespace image
{
    /**
     * \brief Writes uncompressed TGA files.
     *
     * The CoD4 AssetManager refuses block compressed images as compositing sources ("is not a compositable image
     * type") and its dds reader only knows the DXT fourCCs, so TGA is the only format a decomposed source image can
     * be written in. Supported are the two layouts that map onto a TGA file one to one: B8_G8_R8 as a 24 bit true
     * color image and R8 as an 8 bit grayscale image. Only the first mip level is written, TGA has no mip maps.
     */
    class TgaWriter final : public ImageWriter
    {
    public:
        bool SupportsImageFormat(const ImageFormat* imageFormat) override;
        std::string GetFileExtension() override;
        void DumpImage(std::ostream& stream, const Texture* texture) override;
    };
} // namespace image
