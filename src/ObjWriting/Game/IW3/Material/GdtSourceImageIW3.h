#pragma once

#include "Dumping/AssetDumpingContext.h"
#include "Dumping/IZoneAssetDumperState.h"
#include "Game/IW3/IW3.h"

#include <string>
#include <unordered_map>

namespace material
{
    /**
     * \brief Resolves the GDT source image path for a material's texture, writing a TGA when DDS is unsuitable.
     *
     * Normal maps and non-DXT DDS images cannot be read by the CoD4 Asset Manager, so this class writes them as
     * uncompressed TGA files and points the GDT at those instead. Each image is loaded and written at most once
     * per zone, cached by the image name and whether it is used as a normal map.
     */
    class GdtSourceImages final : public IZoneAssetDumperState
    {
    public:
        /**
         * \return The path to put in a gdt source image field, relative to the game root.
         */
        [[nodiscard]] std::string GetSourceImagePath(AssetDumpingContext& context, const IW3::GfxImage& image, bool isNormalMap);

    private:
        std::unordered_map<std::string, std::string> m_paths;
    };
} // namespace material
