#include "MaterialGdtDumperIW3.h"

#include "Game/IW3/MaterialConstantsIW3.h"
#include "Game/IW3/Material/MaterialConstantZoneStateIW3.h"
#include "Game/IW3/ObjConstantsIW3.h"
#include "Game/IW3/Techset/TechsetConstantsIW3.h"
#include "ObjWriting.h"
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
        MaterialGdtDumper(const Material& material, const MaterialConstantZoneState& constants)
            : m_material(material),
              m_constants(constants),
              m_entry(AssetName(material.info.name), GDF_FILENAME_MATERIAL)
        {
        }

        GdtEntry CreateGdtEntry()
        {
            // AssetManager writes the full field set, so start from the gdf defaults and overwrite what the asset knows
            for (const auto& [key, value] : gdtMaterialDefaults)
                m_entry.m_properties[key] = value;

            SetValue("template", GDT_MATERIAL_TEMPLATE);
            SetValue("materialType", GdtMaterialTypeNames[static_cast<size_t>(DetermineMaterialType())]);
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
                if (bits.srcBlendRgb == GFXS_BLEND_SRCALPHA && bits.dstBlendRgb == GFXS_BLEND_INVSRCALPHA)
                    return GdtBlendFuncNames[static_cast<size_t>(GdtBlendFunc_e::BLEND)];
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

            // The gdf only knows the disabled and the GE128 case
            SetValue("alphaTest",
                     !bits.alphaTestDisabled && bits.alphaTest == GFXS_ALPHA_TEST_GE_128 ? GDT_ALPHA_TEST_GE128 : GDT_ALPHA_TEST_ALWAYS);

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

                if (!textureDef.u.image || !textureDef.u.image->name)
                    continue;

                SetValue(knownMap->second.m_gdt_name, SourceImagePath(AssetName(textureDef.u.image->name)));
                SetValue("tile"s + knownMap->second.m_property_suffix, GdtTileModeNames[static_cast<size_t>(GetTileMode(textureDef.samplerState))]);
                SetValue("filter"s + knownMap->second.m_property_suffix, GdtFilterNames[static_cast<size_t>(GetFilter(textureDef.samplerState))]);
                SetValue("format"s + knownMap->second.m_property_suffix, GDT_FORMAT_AUTO);
            }
        }

        const Material& m_material;
        const MaterialConstantZoneState& m_constants;
        GdtEntry m_entry;
    };
} // namespace

namespace material
{
    void GdtDumperIW3::DumpAsset(AssetDumpingContext& context, const XAssetInfo<AssetMaterial::Type>& asset)
    {
        if (!context.m_gdt)
            return;

        auto* constants = context.GetZoneAssetDumperState<MaterialConstantZoneState>();
        MaterialGdtDumper dumper(*asset.Asset(), *constants);
        context.m_gdt->WriteEntry(dumper.CreateGdtEntry());
    }
} // namespace material
