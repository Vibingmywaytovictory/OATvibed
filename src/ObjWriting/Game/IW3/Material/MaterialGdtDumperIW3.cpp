#include "MaterialGdtDumperIW3.h"

#include "Game/IW3/Material/MaterialConstantZoneStateIW3.h"
#include "Game/IW3/MaterialConstantsIW3.h"
#include "Game/IW3/ObjConstantsIW3.h"
#include "Game/IW3/Techset/TechsetConstantsIW3.h"
#include "Image/ImageCommon.h"
#include "Material/MaterialGdtNaming.h"
#include "Material/MaterialGdtZoneState.h"
#include "ObjWriting.h"
#include "SearchPath/ISearchPath.h"
#include "Utils/Logging/Log.h"

#include <format>
#include <string>
#include <type_traits>

using namespace IW3;
using namespace std::string_literals;

namespace
{
    const char* AssetName(const char* name)
    {
        if (name && name[0] == ',')
            return &name[1];
        return name;
    }

    /**
     * \brief Whether an image was produced by the converter rather than authored, so it cannot be a gdt source image.
     *
     * A leading tilde marks an image composited out of several source images, most commonly the specular color and
     * cosine power pair that CoD4 packs into one map. A leading dollar marks an engine built-in such as
     * $identitynormalmap, dumped as a 1x1 placeholder AssetManager rejects with "unknown format 00000000".
     * Neither has recoverable originals.
     */
    bool IsGeneratedImage(const char* imageName)
    {
        return imageName[0] == '~' || imageName[0] == '$';
    }

    // material.gdf expects a source image, and DDS is the only dumpable format AssetManager reads.
    // The path is relative to the game root, which is where the dumped images folder is meant to end up.
    std::string SourceImagePath(const char* imageName)
    {
        if (ObjWriting::Configuration.ImageOutputFormat == ImageOutputFormat_e::DDS)
            return std::format("images/{}.dds", imageName);

        return std::format("images/{}.iwi", imageName);
    }

    class MaterialGdtDumper
    {
    public:
        MaterialGdtDumper(const Material& material, const MaterialConstantZoneState& constants, ISearchPath& searchPath)
            : m_material(material),
              m_constants(constants),
              m_search_path(searchPath),
              // Prefix-stripped: AssetManager derives the mc/ wc/ prefix from materialType itself, and the linker
              // strips it when resolving raw/materials files. A prefixed entry name would double the prefix.
              m_entry(material::StripTechniqueCategoryPrefix(AssetName(material.info.name)), GDF_FILENAME_MATERIAL)
        {
        }

        // A gdt entry that cannot convert aborts the whole "build all" pass of AssetManager, so a material whose
        // source images are not part of this fastfile is better left out than written and known to fail.
        [[nodiscard]] bool CanConvert(std::string& reason)
        {
            if (!m_material.textureTable || m_material.textureCount == 0)
                return true;

            for (auto i = 0u; i < m_material.textureCount; i++)
            {
                const auto& textureDef = m_material.textureTable[i];

                std::string samplerName;
                if (!m_constants.GetTextureDefName(textureDef.nameHash, samplerName) || !gdtMaterialTextureMaps.contains(samplerName))
                    continue;

                if (!textureDef.u.image || !textureDef.u.image->name)
                    continue;

                const auto* imageName = AssetName(textureDef.u.image->name);
                if (IsGeneratedImage(imageName))
                    continue;

                // The dumped image comes from an iwi of the search path, not from the fastfile itself
                if (!m_search_path.Open(image::GetFileNameForAsset(imageName, ".iwi")).IsOpen())
                {
                    reason = std::format("image \"{}\" is not part of this fastfile", imageName);
                    return false;
                }
            }

            return true;
        }

        GdtEntry CreateGdtEntry()
        {
            // AssetManager writes the full field set, so start from the gdf defaults and overwrite what the asset knows
            for (const auto& [key, value] : gdtMaterialDefaults)
                m_entry.m_properties[key] = value;

            const auto materialType = DetermineMaterialType();
            m_is_phong = materialType == GdtMaterialType_e::WORLD_PHONG || materialType == GdtMaterialType_e::MODEL_PHONG;

            SetValue("template", GDT_MATERIAL_TEMPLATE);
            SetValue("materialType", GdtMaterialTypeNames[static_cast<size_t>(materialType)]);
            SetValue("usage", GDT_USAGE_NOT_IN_EDITOR);
            SetValue("sort", GDT_SORT_DEFAULT);
            SetValue("surfaceType", DetermineSurfaceType());

            SetValue("textureAtlasRowCount", std::to_string(m_material.info.textureAtlasRowCount));
            SetValue("textureAtlasColumnCount", std::to_string(m_material.info.textureAtlasColumnCount));

            SetStateBitsValues();
            SetTextureTableValues();

            return m_entry;
        }

    private:
        void SetValue(const std::string& key, std::string value)
        {
            m_entry.m_properties[key] = std::move(value);
        }

        // The engine material type only distinguishes model from world. Everything else has to come from the
        // technique set name, the same way the technique set name is derived from the material type when compiling.
        GdtMaterialType_e DetermineMaterialType() const
        {
            if (!m_material.techniqueSet || !m_material.techniqueSet->name)
                return GdtMaterialType_e::UNKNOWN;

            const std::string techsetName(m_material.techniqueSet->name);

            for (auto materialType = static_cast<size_t>(MTL_TYPE_DEFAULT) + 1u; materialType < MTL_TYPE_COUNT; materialType++)
            {
                const std::string_view prefix(g_materialTypeInfo[materialType].techniqueSetPrefix);
                if (prefix.empty() || techsetName.rfind(prefix, 0) != 0)
                    continue;

                const auto isModel = materialType == MTL_TYPE_MODEL || materialType == MTL_TYPE_MODEL_VERTCOL;
                if (techsetName.find("unlit", prefix.size()) != std::string::npos)
                    return isModel ? GdtMaterialType_e::MODEL_UNLIT : GdtMaterialType_e::WORLD_UNLIT;
                if (isModel && techsetName.find("ambient", prefix.size()) != std::string::npos)
                    return GdtMaterialType_e::MODEL_AMBIENT;

                return isModel ? GdtMaterialType_e::MODEL_PHONG : GdtMaterialType_e::WORLD_PHONG;
            }

            // Prefixless technique sets name their material type outright
            if (techsetName == "2d")
                return GdtMaterialType_e::TWO_D;
            if (techsetName.rfind("effect", 0) == 0)
                return GdtMaterialType_e::EFFECT;
            if (techsetName.rfind("distortion", 0) == 0)
                return GdtMaterialType_e::DISTORTION;
            if (techsetName.rfind("particle", 0) == 0)
                return GdtMaterialType_e::PARTICLE_CLOUD;
            if (techsetName.rfind("unlit", 0) == 0)
                return GdtMaterialType_e::UNLIT;
            if (techsetName.rfind("tools", 0) == 0)
                return GdtMaterialType_e::TOOLS;
            if (techsetName.rfind("sky", 0) == 0)
                return GdtMaterialType_e::SKY;
            if (techsetName.rfind("water", 0) == 0)
                return GdtMaterialType_e::WATER;

            return GdtMaterialType_e::UNKNOWN;
        }

        // surfaceTypeBits is a bitfield while the gdf offers a single value, so only the first type survives
        std::string DetermineSurfaceType() const
        {
            if (!m_material.info.surfaceTypeBits)
                return GDT_SURFACE_TYPE_NONE;

            for (auto surfaceType = 1u; surfaceType < SURF_TYPE_NUM; surfaceType++)
            {
                if (m_material.info.surfaceTypeBits & (1 << (surfaceType - 1)))
                    return surfaceTypeNames[surfaceType];
            }

            return GDT_SURFACE_TYPE_NONE;
        }

        static const char* GetBlendFuncName(const GfxStateBitsLoadBitsStructured& bits)
        {
            if (bits.blendOpRgb == GFXS_BLENDOP_DISABLED || bits.srcBlendRgb == GFXS_BLEND_DISABLED)
                return GdtBlendFuncNames[static_cast<size_t>(GdtBlendFunc_e::REPLACE)];

            if (bits.blendOpRgb == GFXS_BLENDOP_ADD)
            {
                // The lit technique of a blended material premultiplies alpha in the shader and so uses One instead
                // of SrcAlpha, but both come from the same authored blendFunc
                if ((bits.srcBlendRgb == GFXS_BLEND_SRCALPHA || bits.srcBlendRgb == GFXS_BLEND_ONE) && bits.dstBlendRgb == GFXS_BLEND_INVSRCALPHA)
                {
                    return GdtBlendFuncNames[static_cast<size_t>(GdtBlendFunc_e::BLEND)];
                }
                if (bits.srcBlendRgb == GFXS_BLEND_ONE && bits.dstBlendRgb == GFXS_BLEND_ONE)
                    return GdtBlendFuncNames[static_cast<size_t>(GdtBlendFunc_e::ADD)];
                if (bits.srcBlendRgb == GFXS_BLEND_ZERO && bits.dstBlendRgb == GFXS_BLEND_SRCCOLOR)
                    return GdtBlendFuncNames[static_cast<size_t>(GdtBlendFunc_e::MULTIPLY)];
                if (bits.srcBlendRgb == GFXS_BLEND_INVDESTCOLOR && bits.dstBlendRgb == GFXS_BLEND_ONE)
                    return GdtBlendFuncNames[static_cast<size_t>(GdtBlendFunc_e::SCREEN_ADD)];
            }

            return GdtBlendFuncNames[static_cast<size_t>(GdtBlendFunc_e::CUSTOM)];
        }

        template<typename T> static const char* NameForIndex(const T& table, const unsigned index)
        {
            if (index >= std::extent_v<T>)
                return "";

            return table[index];
        }

        void SetStateBitsValues()
        {
            // The lit technique carries the state the gdf describes; fall back to whichever technique the material does have
            auto stateBitsIndex = -1;
            if (m_material.stateBitsEntry[TECHNIQUE_LIT] >= 0 && m_material.stateBitsEntry[TECHNIQUE_LIT] < m_material.stateBitsCount)
            {
                stateBitsIndex = m_material.stateBitsEntry[TECHNIQUE_LIT];
            }
            else
            {
                for (const auto entry : m_material.stateBitsEntry)
                {
                    if (entry >= 0 && entry < m_material.stateBitsCount)
                    {
                        stateBitsIndex = entry;
                        break;
                    }
                }
            }

            if (!m_material.stateBitsTable || m_material.stateBitsCount == 0 || stateBitsIndex < 0)
                return;

            const auto& bits = m_material.stateBitsTable[stateBitsIndex].loadBits.structured;

            const auto* blendFuncName = GetBlendFuncName(bits);
            m_is_additive = blendFuncName == GdtBlendFuncNames[static_cast<size_t>(GdtBlendFunc_e::ADD)];
            SetValue("blendFunc", blendFuncName);
            if (blendFuncName == GdtBlendFuncNames[static_cast<size_t>(GdtBlendFunc_e::CUSTOM)])
            {
                SetValue("customBlendOpRgb", NameForIndex(GdtBlendOpNames, bits.blendOpRgb));
                SetValue("srcCustomBlendFunc", NameForIndex(GdtCustomBlendFuncNames, bits.srcBlendRgb));
                SetValue("destCustomBlendFunc", NameForIndex(GdtCustomBlendFuncNames, bits.dstBlendRgb));
            }

            SetValue("customBlendOpAlpha", NameForIndex(GdtBlendOpNames, bits.blendOpAlpha));
            SetValue("srcCustomBlendFuncAlpha", NameForIndex(GdtCustomBlendFuncNames, bits.srcBlendAlpha));
            SetValue("destCustomBlendFuncAlpha", NameForIndex(GdtCustomBlendFuncNames, bits.dstBlendAlpha));

            // "Always" is the no alpha test case, so every enabled variant has to map onto the single GE128 the gdf
            // offers. Emitting Always for an enabled test would both lose the test and, together with a Custom
            // blendFunc, be rejected outright ("not supported for phong materials").
            SetValue("alphaTest", bits.alphaTestDisabled ? GDT_ALPHA_TEST_ALWAYS : GDT_ALPHA_TEST_GE128);

            if (bits.depthTestDisabled)
            {
                SetValue("depthTest", GDT_DEPTH_TEST_DISABLE);
            }
            else
            {
                switch (bits.depthTest)
                {
                case GFXS_DEPTHTEST_ALWAYS:
                    SetValue("depthTest", GDT_DEPTH_TEST_ALWAYS);
                    break;
                case GFXS_DEPTHTEST_LESS:
                    SetValue("depthTest", GDT_DEPTH_TEST_LESS);
                    break;
                case GFXS_DEPTHTEST_EQUAL:
                    SetValue("depthTest", GDT_DEPTH_TEST_EQUAL);
                    break;
                default:
                    SetValue("depthTest", GDT_DEPTH_TEST_LESS_EQUAL);
                    break;
                }
            }

            SetValue("depthWrite", bits.depthWrite ? GDT_DEPTH_WRITE_ON : GDT_DEPTH_WRITE_OFF);
            SetValue("cullFace", NameForIndex(GdtCullFaceNames, bits.cullFace));
            SetValue("polygonOffset", NameForIndex(GdtPolygonOffsetNames, bits.polygonOffset));

            const auto* colorWriteRgb = bits.colorWriteRgb ? GDT_ENABLE : GDT_DISABLE;
            SetValue("colorWriteRed", colorWriteRgb);
            SetValue("colorWriteGreen", colorWriteRgb);
            SetValue("colorWriteBlue", colorWriteRgb);
            SetValue("colorWriteAlpha", bits.colorWriteAlpha ? GDT_ENABLE : GDT_DISABLE);
        }

        static GdtTileMode_e GetTileMode(const MaterialTextureDefSamplerState& samplerState)
        {
            if (samplerState.clampU && samplerState.clampV)
                return GdtTileMode_e::NO_TILE;
            if (samplerState.clampU)
                return GdtTileMode_e::TILE_VERTICAL;
            if (samplerState.clampV)
                return GdtTileMode_e::TILE_HORIZONTAL;

            return GdtTileMode_e::TILE_BOTH;
        }

        static GdtFilter_e GetFilter(const MaterialTextureDefSamplerState& samplerState)
        {
            switch (samplerState.filter)
            {
            case TEXTURE_FILTER_ANISO2X:
                return samplerState.mipMap == SAMPLER_MIPMAP_ENUM_LINEAR ? GdtFilter_e::MIP_2X_TRILINEAR : GdtFilter_e::MIP_2X_BILINEAR;
            case TEXTURE_FILTER_ANISO4X:
                return samplerState.mipMap == SAMPLER_MIPMAP_ENUM_LINEAR ? GdtFilter_e::MIP_4X_TRILINEAR : GdtFilter_e::MIP_4X_BILINEAR;
            case TEXTURE_FILTER_NEAREST:
                return GdtFilter_e::NOMIP_NEAREST;
            case TEXTURE_FILTER_LINEAR:
                return GdtFilter_e::NOMIP_BILINEAR;
            default:
                return GdtFilter_e::AUTO;
            }
        }

        void SetTextureTableValues()
        {
            if (!m_material.textureTable || m_material.textureCount == 0)
                return;

            for (auto i = 0u; i < m_material.textureCount; i++)
            {
                const auto& textureDef = m_material.textureTable[i];

                std::string samplerName;
                if (!m_constants.GetTextureDefName(textureDef.nameHash, samplerName))
                {
                    con::warn("Cannot map texture with hash 0x{:x} of material \"{}\" to a gdt field", textureDef.nameHash, m_material.info.name);
                    continue;
                }

                const auto knownMap = gdtMaterialTextureMaps.find(samplerName);
                if (knownMap == gdtMaterialTextureMaps.end())
                    continue;

                // AssetManager rejects the combination outright: "detail map not allowed on additive phong materials"
                if (m_is_additive && m_is_phong && samplerName == "detailMap")
                    continue;

                if (!textureDef.u.image || !textureDef.u.image->name)
                    continue;

                const auto* imageName = AssetName(textureDef.u.image->name);

                // A leading tilde marks an image the converter composited out of several source images, most commonly
                // the specular color and cosine power pair that CoD4 packs into one map. AssetManager cannot take one
                // back as a source ("is not a compositable image type") and the originals are not recoverable, so the
                // map is left unset rather than pointing at something that fails to convert.
                if (IsGeneratedImage(imageName))
                {
                    con::warn("Material \"{}\" uses generated image \"{}\" as {}, which cannot be a gdt source image",
                              m_material.info.name,
                              imageName,
                              knownMap->second.m_gdt_name);
                    continue;
                }

                SetValue(knownMap->second.m_gdt_name, SourceImagePath(imageName));
                SetValue("tile"s + knownMap->second.m_property_suffix, GdtTileModeNames[static_cast<size_t>(GetTileMode(textureDef.samplerState))]);
                SetValue("filter"s + knownMap->second.m_property_suffix, GdtFilterNames[static_cast<size_t>(GetFilter(textureDef.samplerState))]);
                SetValue("format"s + knownMap->second.m_property_suffix, GDT_FORMAT_AUTO);
            }
        }

        const Material& m_material;
        const MaterialConstantZoneState& m_constants;
        ISearchPath& m_search_path;
        GdtEntry m_entry;
        bool m_is_additive = false;
        bool m_is_phong = false;
    };
} // namespace

namespace material
{
    void GdtDumperIW3::Dump(AssetDumpingContext& context)
    {
        // Texture def names are resolved from the shaders of the zone, do not rely on another dumper having done it
        context.GetZoneAssetDumperState<MaterialConstantZoneState>()->EnsureInitialized();

        AbstractAssetDumper::Dump(context);
    }

    void GdtDumperIW3::DumpAsset(AssetDumpingContext& context, const XAssetInfo<AssetMaterial::Type>& asset)
    {
        if (!context.m_gdt)
            return;

        auto* constants = context.GetZoneAssetDumperState<MaterialConstantZoneState>();
        MaterialGdtDumper dumper(*asset.Asset(), *constants, context.m_obj_search_path);

        std::string reason;
        if (!dumper.CanConvert(reason))
        {
            if (ObjWriting::Configuration.GdtSkipUnconvertible)
            {
                con::warn("Skipping material \"{}\" in gdt: {}", asset.m_name, reason);
                return;
            }

            con::warn("Material \"{}\" will not convert: {}", asset.m_name, reason);
        }

        const auto entry = dumper.CreateGdtEntry();
        if (!context.GetZoneAssetDumperState<GdtMaterials>()->Add(entry.m_name))
        {
            // A zone can hold mc/ and wc/ variants of the same material; both compile from one entry natively.
            con::warn("Skipping material \"{}\" in gdt: an entry named \"{}\" was already written", asset.m_name, entry.m_name);
            return;
        }
        context.m_gdt->WriteEntry(entry);
    }
} // namespace material
