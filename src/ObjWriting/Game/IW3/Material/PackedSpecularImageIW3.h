#pragma once

#include "Dumping/AssetDumpingContext.h"
#include "Dumping/IZoneAssetDumperState.h"
#include "Game/IW3/IW3.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace material
{
    /**
     * \brief The two source images a packed specular image was composited from.
     *
     * Either name is empty when the material had no map of that kind - AssetManager substitutes a built-in then.
     */
    struct PackedSpecularSources
    {
        std::string m_spec_color_map;
        std::string m_cosine_power_map;
    };

    /**
     * \brief Derives the names to give the two source images a packed specular image is split back into.
     *
     * AssetManager names a composited image after its sources, so the names can usually be read straight back out of
     * it: "~<spec>-rgb&<cos>-l-11" for a material with both maps and "~<spec>-rgbl-11" for one that used a single
     * image for both. Names that grew too long are cut short and hashed, and a per channel scale is written into the
     * name for materials that did not use the default strengths; in both cases nothing is recoverable and the names
     * are derived from the packed name itself instead.
     */
    [[nodiscard]] PackedSpecularSources GetPackedSpecularSourceNames(std::string_view packedImageName);

    /**
     * \brief Undoes the scale AssetManager applies to the cosine power while compositing.
     *
     * With both specular strengths at their gdf default of 100 the packed alpha comes out as
     * (cosinePower * 61 + 32) / 64, so the source has to be written pre-scaled for the recomposited image to match the
     * packed one. Every value a packed image can hold survives the round trip unchanged.
     */
    [[nodiscard]] std::uint8_t UnscaleCosinePower(std::uint8_t packedValue);

    /**
     * \brief Splits the images CoD4 composited out of a specular color and a cosine power map back into two source
     * images that AssetManager can composite again.
     *
     * The fastfile only holds the packed result, which AssetManager refuses as a source ("is not a compositable image
     * type"), so materials using one would lose their specular map entirely. Several materials commonly share a
     * packed image, so the result of a split is kept and reused.
     */
    class PackedSpecularImages final : public IZoneAssetDumperState
    {
    public:
        /**
         * \return The names the source images were written under, or nullptr when the packed image could not be split.
         */
        [[nodiscard]] const PackedSpecularSources*
            GetDecomposedSources(AssetDumpingContext& context, const std::string& packedImageName, const IW3::GfxImage& packedImage);

    private:
        std::unordered_map<std::string, std::optional<PackedSpecularSources>> m_decomposed;
    };
} // namespace material
