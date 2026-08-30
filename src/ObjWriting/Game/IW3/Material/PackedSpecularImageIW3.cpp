#include "PackedSpecularImageIW3.h"

#include "Game/IW3/Image/ImageToCommonConverterIW3.h"
#include "Image/Compression/ImageDecompressor.h"
#include "Image/ImageCommon.h"
#include "Image/TextureConverter.h"
#include "Image/TgaWriter.h"
#include "Utils/Logging/Log.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <format>
#include <memory>

using namespace IW3;
using namespace image;

namespace
{
    constexpr std::string_view CHANNELS_SPEC_COLOR = "-rgb";
    constexpr std::string_view CHANNELS_COSINE_POWER = "-l";
    constexpr std::string_view CHANNELS_BOTH = "-rgbl";
    constexpr std::string_view COSINE_POWER_NAME_SUFFIX = "_cos";

    // AssetManager scales the cosine power by this when compositing, with the strengths at their gdf default of 100.
    // Verified against converted images for every input value: packed = (cosinePower * 61 + 32) / 64.
    constexpr unsigned COSINE_POWER_SCALE_NUMERATOR = 61u;
    constexpr unsigned COSINE_POWER_SCALE_DENOMINATOR = 64u;

    // AssetManager refuses a source image whose converted iwi file name is "longer than 42 characters". It is the same
    // budget its own composited names are cut down to, which is why those end in a hash.
    constexpr auto MAX_IMAGE_NAME_LENGTH = 38uz;
    constexpr auto NAME_HASH_LENGTH = 9uz;

    bool IsPlainImageName(const std::string_view name)
    {
        return !name.empty()
               && std::ranges::all_of(name,
                                      [](const char c)
                                      {
                                          return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.' || c == '/';
                                      });
    }

    bool IsNumber(const std::string_view str)
    {
        return !str.empty()
               && std::ranges::all_of(str,
                                      [](const char c)
                                      {
                                          return std::isdigit(static_cast<unsigned char>(c)) != 0;
                                      });
    }

    // An engine built-in stands in for a map the material did not have
    std::string SourceNameOfPart(const std::string_view name)
    {
        if (name.starts_with('$'))
            return {};

        return std::string(name);
    }

    /**
     * \brief Cuts a name down to what AssetManager accepts, keeping it unique by ending it in a hash of the original.
     */
    std::string ShortenImageName(std::string name)
    {
        if (name.size() <= MAX_IMAGE_NAME_LENGTH)
            return name;

        // FNV-1a rather than std::hash so that the same dump always produces the same names
        auto hash = 0x811C9DC5u;
        for (const auto c : name)
        {
            hash ^= static_cast<unsigned char>(c);
            hash *= 0x01000193u;
        }

        return std::format("{}_{:08x}", std::string_view(name).substr(0uz, MAX_IMAGE_NAME_LENGTH - NAME_HASH_LENGTH), hash);
    }

    /**
     * \brief Turns a packed image name into something usable as a file name of its own.
     */
    std::string SanitizeImageName(const std::string_view name)
    {
        std::string sanitized;
        sanitized.reserve(name.size());

        for (const auto c : name)
        {
            if (std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_')
                sanitized.push_back(c);
            else if (!sanitized.empty() && sanitized.back() != '_')
                sanitized.push_back('_');
        }

        while (!sanitized.empty() && sanitized.back() == '_')
            sanitized.pop_back();

        return sanitized;
    }

    material::PackedSpecularSources MakeSources(std::string specColorMap, std::string cosinePowerMap)
    {
        return {ShortenImageName(std::move(specColorMap)), ShortenImageName(std::move(cosinePowerMap))};
    }

    /**
     * \brief Brings the pixels of a packed image into a layout that can be read channel by channel.
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
        const auto file = context.OpenAssetFile(GetFileNameForAsset(imageName, ".tga"));
        if (!file)
            return false;

        TgaWriter().DumpImage(*file, &texture);

        return true;
    }

    bool WriteSpecColorMap(AssetDumpingContext& context, const std::string& imageName, const Texture& packedPixels)
    {
        Texture2D specColorMap(&format::B8_G8_R8, packedPixels.GetWidth(), packedPixels.GetHeight());
        specColorMap.Allocate();

        const auto* in = packedPixels.GetBufferForMipLevel(0);
        auto* out = specColorMap.GetBufferForMipLevel(0, 0);
        const auto pixelCount = static_cast<size_t>(packedPixels.GetWidth()) * packedPixels.GetHeight();

        for (auto pixel = 0uz; pixel < pixelCount; pixel++)
        {
            out[pixel * 3uz + 0uz] = in[pixel * 4uz + 0uz];
            out[pixel * 3uz + 1uz] = in[pixel * 4uz + 1uz];
            out[pixel * 3uz + 2uz] = in[pixel * 4uz + 2uz];
        }

        return WriteTga(context, imageName, specColorMap);
    }

    bool WriteCosinePowerMap(AssetDumpingContext& context, const std::string& imageName, const Texture& packedPixels)
    {
        // AssetManager takes the luminance of the cosine power source, which a grayscale image gives back unchanged
        Texture2D cosinePowerMap(&format::R8, packedPixels.GetWidth(), packedPixels.GetHeight());
        cosinePowerMap.Allocate();

        const auto* in = packedPixels.GetBufferForMipLevel(0);
        auto* out = cosinePowerMap.GetBufferForMipLevel(0, 0);
        const auto pixelCount = static_cast<size_t>(packedPixels.GetWidth()) * packedPixels.GetHeight();

        for (auto pixel = 0uz; pixel < pixelCount; pixel++)
            out[pixel] = material::UnscaleCosinePower(in[pixel * 4uz + 3uz]);

        return WriteTga(context, imageName, cosinePowerMap);
    }
} // namespace

namespace material
{
    std::uint8_t UnscaleCosinePower(const std::uint8_t packedValue)
    {
        const auto unscaled =
            (static_cast<unsigned>(packedValue) * COSINE_POWER_SCALE_DENOMINATOR + COSINE_POWER_SCALE_NUMERATOR / 2u) / COSINE_POWER_SCALE_NUMERATOR;

        return static_cast<std::uint8_t>(std::min(unscaled, 255u));
    }

    PackedSpecularSources GetPackedSpecularSourceNames(const std::string_view packedImageName)
    {
        auto name = packedImageName;
        if (name.starts_with('~'))
            name.remove_prefix(1);

        // A name that grew past the converter's limit was cut short and hashed, which loses the source names
        if (name.find('~') == std::string_view::npos)
        {
            // The trailing number encodes the strengths the material was compiled with
            const auto suffixSeparator = name.find_last_of('-');
            if (suffixSeparator != std::string_view::npos && IsNumber(name.substr(suffixSeparator + 1uz)))
                name = name.substr(0uz, suffixSeparator);

            const auto sourceSeparator = name.find('&');
            if (sourceSeparator != std::string_view::npos)
            {
                const auto specPart = name.substr(0uz, sourceSeparator);
                const auto cosinePart = name.substr(sourceSeparator + 1uz);

                if (specPart.ends_with(CHANNELS_SPEC_COLOR) && cosinePart.ends_with(CHANNELS_COSINE_POWER))
                {
                    auto specName = SourceNameOfPart(specPart.substr(0uz, specPart.size() - CHANNELS_SPEC_COLOR.size()));
                    auto cosineName = SourceNameOfPart(cosinePart.substr(0uz, cosinePart.size() - CHANNELS_COSINE_POWER.size()));

                    // A built-in leaves its name empty, and an image composited from nothing but built-ins has no
                    // source to write at all
                    if (specName.empty() && cosineName.empty())
                        return {};

                    if ((specName.empty() || IsPlainImageName(specName)) && (cosineName.empty() || IsPlainImageName(cosineName)))
                        return MakeSources(std::move(specName), std::move(cosineName));
                }
            }
            else if (name.ends_with(CHANNELS_BOTH))
            {
                // One image supplied both the color and, through its luminance, the cosine power. Two files are
                // written anyway: rebuilding the cosine power out of the color channels would only come out right if
                // the original source really was consistent, while the packed alpha is what the material renders with.
                const auto sourceName = SourceNameOfPart(name.substr(0uz, name.size() - CHANNELS_BOTH.size()));

                if (sourceName.empty())
                    return {};

                if (IsPlainImageName(sourceName))
                    return MakeSources(sourceName, sourceName + std::string(COSINE_POWER_NAME_SUFFIX));
            }
        }

        // Nothing recoverable, so derive both names from the packed name to keep them unique and stable
        const auto fallbackName = SanitizeImageName(packedImageName);
        if (fallbackName.empty())
            return {};

        return MakeSources(fallbackName, fallbackName + std::string(COSINE_POWER_NAME_SUFFIX));
    }

    const PackedSpecularSources*
        PackedSpecularImages::GetDecomposedSources(AssetDumpingContext& context, const std::string& packedImageName, const GfxImage& packedImage)
    {
        const auto existingEntry = m_decomposed.find(packedImageName);
        if (existingEntry != m_decomposed.end())
            return existingEntry->second ? &*existingEntry->second : nullptr;

        // Remember failures as well so that the next material using the image does not run into them again
        auto& sources = m_decomposed[packedImageName];

        auto names = GetPackedSpecularSourceNames(packedImageName);
        if (names.m_spec_color_map.empty() && names.m_cosine_power_map.empty())
            return nullptr;

        auto texture = ToCommonConverterIW3::LoadTexture(packedImage, context.m_obj_search_path);
        if (!texture)
            return nullptr;

        auto packedPixels = ConvertToBgra(std::move(texture));
        if (!packedPixels)
        {
            con::warn("Cannot split packed image \"{}\" into source images: unsupported image format", packedImageName);
            return nullptr;
        }

        if (!names.m_spec_color_map.empty() && !WriteSpecColorMap(context, names.m_spec_color_map, *packedPixels))
            return nullptr;

        if (!names.m_cosine_power_map.empty() && !WriteCosinePowerMap(context, names.m_cosine_power_map, *packedPixels))
            return nullptr;

        sources = std::move(names);

        return &*sources;
    }
} // namespace material
