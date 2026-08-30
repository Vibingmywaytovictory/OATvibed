#include "GdtSourceImageIW3.h"

#include "Game/IW3/Image/ImageToCommonConverterIW3.h"
#include "Image/Compression/ImageDecompressor.h"
#include "Image/ImageCommon.h"
#include "Image/TextureConverter.h"
#include "Image/TgaWriter.h"
#include "ObjWriting.h"
#include "Utils/Logging/Log.h"

#include <algorithm>
#include <cstdint>
#include <format>
#include <memory>

using namespace IW3;
using namespace image;

namespace
{
    /**
     * \brief Brings the pixels of a texture into a layout that TGA can write directly.
     */
    std::unique_ptr<Texture> ConvertToBgra(std::unique_ptr<Texture> texture)
    {
        const auto* textureFormat = texture->GetFormat();
        if (textureFormat->GetId() == ImageFormatId::B8_G8_R8_A8)
            return texture;

        if (textureFormat->GetType() == ImageFormatType::BLOCK_COMPRESSED)
        {
            auto* decompressor = ImageDecompressor::GetDecompressorForFormat(textureFormat->GetId());
            if (!decompressor)
                return nullptr;

            return decompressor->Decompress(*texture, &format::B8_G8_R8_A8);
        }

        TextureConverter converter(texture.get(), &format::B8_G8_R8_A8);
        return converter.Convert();
    }

    bool WriteTga(AssetDumpingContext& context, const std::string& imageName, const Texture& texture)
    {
        // The writer silently produces an empty file for a format it cannot encode, which AssetManager then rejects
        // with a message about the tga rather than about the image, so refuse here instead
        if (!TgaWriter().SupportsImageFormat(texture.GetFormat()))
            return false;

        const auto file = context.OpenAssetFile(GetFileNameForAsset(imageName, ".tga"));
        if (!file)
            return false;

        TgaWriter().DumpImage(*file, &texture);

        return true;
    }

    // AssetManager's dds reader dispatches on the fourCC and knows only these three
    bool IsDxtFormat(const ImageFormatId formatId)
    {
        return formatId == ImageFormatId::BC1 || formatId == ImageFormatId::BC2 || formatId == ImageFormatId::BC3;
    }

    // A leading comma marks the name of an asset the zone only references
    const char* AssetName(const char* name)
    {
        if (name && name[0] == ',')
            return &name[1];
        return name;
    }
} // namespace

namespace material
{
    std::string GdtSourceImages::GetSourceImagePath(AssetDumpingContext& context, const GfxImage& image, bool isNormalMap)
    {
        const std::string name(AssetName(image.name));
        const auto key = std::format("{}|{}", name, isNormalMap ? '1' : '0');

        const auto existingEntry = m_paths.find(key);
        if (existingEntry != m_paths.end())
            return existingEntry->second;

        auto& path = m_paths[key];

        if (ObjWriting::Configuration.ImageOutputFormat != ImageOutputFormat_e::DDS)
        {
            path = GetFileNameForAsset(name, ".iwi");
            return path;
        }

        auto texture = ToCommonConverterIW3::LoadTexture(image, context.m_obj_search_path);
        if (!texture)
        {
            path = GetFileNameForAsset(name, ".dds");
            return path;
        }

        const auto* format = texture->GetFormat();

        if (!isNormalMap && IsDxtFormat(format->GetId()))
        {
            path = GetFileNameForAsset(name, ".dds");
            return path;
        }

        auto bgra = ConvertToBgra(std::move(texture));
        if (!bgra)
        {
            con::warn("Cannot write TGA for image \"{}\": unsupported image format, falling back to DDS", name);
            path = GetFileNameForAsset(name, ".dds");
            return path;
        }

        if (!WriteTga(context, name, *bgra))
        {
            con::warn("Cannot write TGA for image \"{}\", falling back to DDS", name);
            path = GetFileNameForAsset(name, ".dds");
            return path;
        }

        path = GetFileNameForAsset(name, ".tga");
        return path;
    }
} // namespace material
