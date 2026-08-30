#include "MaterialGdtDumperIW3.h"

#include "Dumping/GdtOutputStreamCollection.h"
#include "Game/IW3/Material/GdtSourceImageIW3.h"
#include "Game/IW3/Material/MaterialConstantZoneStateIW3.h"
#include "Game/IW3/Material/PackedSpecularImageIW3.h"
#include "Game/IW3/MaterialConstantsIW3.h"
#include "Game/IW3/ObjConstantsIW3.h"
#include "Game/IW3/Techset/TechsetConstantsIW3.h"
#include "Image/ImageCommon.h"
#include "Material/MaterialGdtNaming.h"
#include "Material/MaterialGdtZoneState.h"
#include "ObjWriting.h"
#include "SearchPath/ISearchPath.h"
#include "Utils/Logging/Log.h"

#include <cmath>
#include <cstring>
#include <format>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>

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

    // The sampler CoD4 feeds the packed specular color and cosine power map into
    constexpr auto SAMPLER_SPECULAR = "specularMap";

    // A material whose technique set is none of the ones a built-in material type produces was authored as "custom"
    // with a template of its own. deffiles/materials/mtl_<name>.template names the technique set it compiles to, so
    // the mapping is one entry per template that CoD4 ships.
    const std::unordered_map<std::string_view, std::string_view> customTemplatesByTechniqueSet{
        {"reflexsight",         "mtl_reflexsight"        },
        {"grain_overlay",       "mtl_grain_overlay"      },
        {"clear_alpha_stencil", "mtl_clear_alpha_stencil"},
        {"shadowclear",         "mtl_shadowclear"        },
        {"shadowoverlay",       "mtl_shadowoverlay"      },
        {"shadowcookieblur",    "mtl_shadowcookieblur"   },
        {"shadowcookieoverlay", "mtl_shadowcookieoverlay"},
        {"effect_add_eyeoffset", "mtl_effect_eyeoffset"  },
        {"l_sm_flag_t0c0n0s0",  "mtl_phong_flag"         },
        {"unlit_replace",       "mtl_unlit_deprecated"   },
    };

    // The technique set name carries the mc_/wc_ material category, which is not part of the template's own name
    std::string_view StripTechniqueSetCategory(const std::string_view techsetName)
    {
        for (const auto prefix : {"mc_", "wc_"})
        {
            if (techsetName.starts_with(prefix))
                return techsetName.substr(3);
        }

        return techsetName;
    }

    // A gdt float field holds an ordinary decimal; trailing zeroes only make the entry differ from a hand authored one
    std::string GdtFloat(const float value)
    {
        auto str = std::format("{}", value);
        if (str.find('.') == std::string::npos && str.find('e') == std::string::npos)
            str += ".0";

        return str;
    }

    /**
     * \brief A leading tilde marks an image the converter composited out of several source images.
     *
     * The specular color and cosine power pair CoD4 packs into one map is the only case that occurs in practice, and
     * it is split back into two source images. Anything else has no recoverable originals.
     */
    bool IsPackedImage(const char* imageName)
    {
        return imageName[0] == '~';
    }

    /**
     * \brief A leading dollar marks an engine built-in such as $identitynormalmap.
     *
     * They stand in for a map the material did not have and are dumped as 1x1 placeholders AssetManager rejects with
     * "unknown format 00000000", so the gdt field is left unset rather than pointed at one.
     */
    bool IsBuiltInImage(const char* imageName)
    {
        return imageName[0] == '$';
    }

    class MaterialGdtDumper
    {
    public:
        MaterialGdtDumper(AssetDumpingContext& context, const Material& material, const MaterialConstantZoneState& constants)
            : m_context(context),
              m_material(material),
              m_constants(constants),
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
                if (IsBuiltInImage(imageName))
                    continue;

                // A packed image other than the specular one is left out of the entry, so its data is not needed
                if (IsPackedImage(imageName) && samplerName != SAMPLER_SPECULAR)
                    continue;

                // The dumped image comes from an iwi of the search path, not from the fastfile itself
                if (!m_context.m_obj_search_path.Open(image::GetFileNameForAsset(imageName, ".iwi")).IsOpen())
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
            if (!m_custom_template.empty())
                SetValue("customTemplate", m_custom_template);

            // The technique set of a fogless material is the fogged one with a suffix, there is no other trace of it
            if (m_material.techniqueSet && m_material.techniqueSet->name && std::string_view(m_material.techniqueSet->name).contains("nofog"))
                SetValue("noFog", "1");
            SetValue("usage", GDT_USAGE_NOT_IN_EDITOR);
            SetValue("sort", DetermineSort());
            SetValue("surfaceType", DetermineSurfaceType());

            SetValue("textureAtlasRowCount", std::to_string(m_material.info.textureAtlasRowCount));
            SetValue("textureAtlasColumnCount", std::to_string(m_material.info.textureAtlasColumnCount));

            SetStateBitsValues();
            SetConstantValues();
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
        GdtMaterialType_e DetermineMaterialType()
        {
            if (!m_material.techniqueSet || !m_material.techniqueSet->name)
                return GdtMaterialType_e::UNKNOWN;

            const std::string techsetName(m_material.techniqueSet->name);

            // A custom template has to be recognised before the mc_/wc_ prefixes are, or a reflex sight looks like an
            // ordinary model phong material and loses the technique that makes its reticle work
            const auto customTemplate = customTemplatesByTechniqueSet.find(StripTechniqueSetCategory(techsetName));
            if (customTemplate != customTemplatesByTechniqueSet.end())
            {
                m_custom_template = customTemplate->second;
                return GdtMaterialType_e::CUSTOM;
            }

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

        /**
         * \brief The sort the material was authored with, where the gdf can name it.
         *
         * Leaving it at the default makes AssetManager derive a sort from the blend func, which is right for most
         * materials but silently demotes the ones that were sorted by hand - a reflex sight sorted as a viewmodel
         * effect comes back as an ordinary blended surface and draws in the wrong order.
         */
        [[nodiscard]] std::string DetermineSort() const
        {
            const auto sortName = GdtSortNames.find(m_material.info.sortKey);
            if (sortName == GdtSortNames.end())
                return GDT_SORT_DEFAULT;

            return std::string(sortName->second);
        }

        /**
         * \brief The literal of a constant of the material, by the name the shaders of the zone know it under.
         *
         * The name is only stored in full when it fits the twelve inline characters; anything longer is matched by
         * its hash instead.
         */
        [[nodiscard]] const vec4_t* FindConstant(const std::string_view constantName) const
        {
            if (!m_material.constantTable)
                return nullptr;

            for (auto i = 0u; i < m_material.constantCount; i++)
            {
                const auto& constantDef = m_material.constantTable[i];

                const auto fragmentLength = strnlen(constantDef.name, std::extent_v<decltype(MaterialConstantDef::name)>);
                const std::string_view nameFragment(constantDef.name, fragmentLength);

                if (fragmentLength < std::extent_v<decltype(MaterialConstantDef::name)>)
                {
                    if (nameFragment == constantName)
                        return &constantDef.literal;
                    continue;
                }

                std::string knownConstantName;
                if (m_constants.GetConstantName(constantDef.nameHash, knownConstantName) && knownConstantName == constantName)
                    return &constantDef.literal;
            }

            return nullptr;
        }

        /**
         * \brief Puts back the gdf fields that only survive compilation inside a shader constant.
         *
         * Left out, every one of them falls back to the gdf default, which is how a hand tuned lens ends up with the
         * stock environment map response and a soft particle stops feathering against the depth buffer.
         */
        void SetConstantValues()
        {
            // envMapParms holds the min and max scaled by four, measured by converting one material per value
            if (const auto* envMapParms = FindConstant("envMapParms"))
            {
                SetValue("envMapMin", GdtFloat(envMapParms->v[0] / 4.0f));
                SetValue("envMapMax", GdtFloat(envMapParms->v[1] / 4.0f));
                SetValue("envMapExponent", GdtFloat(envMapParms->v[2]));
            }

            // featherParms is (1 / zFeatherDepth, zFeatherDepth, 0, 0) and only exists on a material that feathers
            if (const auto* featherParms = FindConstant("featherParms"))
            {
                SetValue("zFeather", "1");
                SetValue("zFeatherDepth", std::to_string(static_cast<int>(std::lround(featherParms->v[1]))));
            }

            if (const auto* distortionScale = FindConstant("distortionScale"))
            {
                SetValue("distortionScaleX", GdtFloat(distortionScale->v[0]));
                SetValue("distortionScaleY", GdtFloat(distortionScale->v[1]));
            }

            if (const auto* detailScale = FindConstant("detailScale"))
            {
                SetValue("detailScaleX", GdtFloat(detailScale->v[0]));
                SetValue("detailScaleY", GdtFloat(detailScale->v[1]));
            }
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

                // The lit technique of a blended material premultiplies alpha in the shader and so uses One in place
                // of SrcAlpha. What tells that apart from a One that was authored is the alpha test: only a material
                // whose own blend func is SrcAlpha over InvSrcAlpha gets GT0 put on it by raw/statemaps/default.sm.
                if (bits.srcBlendRgb == GFXS_BLEND_ONE && bits.dstBlendRgb == GFXS_BLEND_INVSRCALPHA && !bits.alphaTestDisabled
                    && bits.alphaTest == GFXS_ALPHA_TEST_GT_0)
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

            // The gdf offers only Always and GE128, but the compiled state bits hold three enabled variants. Only
            // GE128 is authored directly; GT0 is put back by raw/statemaps/default.sm, which turns an Always material
            // that blends SrcAlpha over InvSrcAlpha (or over One) into a GT0 test. Writing GE128 for a GT0 material
            // therefore does not merely lose precision, it replaces a "skip fully transparent texels" test with a
            // punchout at half alpha and every soft-edged transparency - lens reticles, glass, fx - renders wrong.
            // LT128 has no authorable form at all and falls back to Always.
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

                // AssetManager rejects the combination outright: "detail map not allowed on additive phong materials".
                // A custom template is not a phong material and mtl_reflexsight even refuses to convert without one.
                if (m_is_additive && m_is_phong && m_custom_template.empty() && samplerName == "detailMap")
                    continue;

                if (!textureDef.u.image || !textureDef.u.image->name)
                    continue;

                const auto* imageName = AssetName(textureDef.u.image->name);

                if (IsBuiltInImage(imageName))
                {
                    con::warn("Material \"{}\" uses built-in image \"{}\" as {}, which cannot be a gdt source image",
                              m_material.info.name,
                              imageName,
                              knownMap->second.m_gdt_name);
                    continue;
                }

                if (IsPackedImage(imageName))
                {
                    if (!SetPackedSpecularValues(samplerName, imageName, *textureDef.u.image, knownMap->second))
                        continue;
                }
                else
                {
                    SetValue(knownMap->second.m_gdt_name,
                             m_context.GetZoneAssetDumperState<material::GdtSourceImages>()
                                 ->GetSourceImagePath(m_context, *textureDef.u.image, samplerName == "normalMap"));
                }

                SetSamplerStateValues(knownMap->second, textureDef.samplerState);
            }
        }

        /**
         * \brief Points the specular fields at the source images the packed image was split back into.
         *
         * A packed image cannot be a gdt source itself ("is not a compositable image type"), so the specular color and
         * cosine power it holds are written out separately and AssetManager packs them again when converting.
         */
        [[nodiscard]] bool
            SetPackedSpecularValues(const std::string& samplerName, const char* imageName, const GfxImage& image, const GdtMaterialTextureMap& gdtMap)
        {
            if (samplerName != SAMPLER_SPECULAR)
            {
                con::warn(
                    "Material \"{}\" uses packed image \"{}\" as {}, which cannot be a gdt source image", m_material.info.name, imageName, gdtMap.m_gdt_name);
                return false;
            }

            const auto* sources = m_context.GetZoneAssetDumperState<material::PackedSpecularImages>()->GetDecomposedSources(m_context, imageName, image);
            if (!sources)
            {
                con::warn("Material \"{}\" uses packed image \"{}\" as {} that could not be split into source images",
                          m_material.info.name,
                          imageName,
                          gdtMap.m_gdt_name);
                return false;
            }

            if (!sources->m_spec_color_map.empty())
                SetValue(gdtMap.m_gdt_name, image::GetFileNameForAsset(sources->m_spec_color_map, ".tga"));

            if (!sources->m_cosine_power_map.empty())
                SetValue("cosinePowerMap", image::GetFileNameForAsset(sources->m_cosine_power_map, ".tga"));

            return true;
        }

        void SetSamplerStateValues(const GdtMaterialTextureMap& gdtMap, const MaterialTextureDefSamplerState& samplerState)
        {
            SetValue("tile"s + gdtMap.m_property_suffix, GdtTileModeNames[static_cast<size_t>(GetTileMode(samplerState))]);
            SetValue("filter"s + gdtMap.m_property_suffix, GdtFilterNames[static_cast<size_t>(GetFilter(samplerState))]);
            SetValue("format"s + gdtMap.m_property_suffix, GDT_FORMAT_AUTO);
        }

        AssetDumpingContext& m_context;
        const Material& m_material;
        const MaterialConstantZoneState& m_constants;
        GdtEntry m_entry;
        std::string m_custom_template;
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
        MaterialGdtDumper dumper(context, *asset.Asset(), *constants);

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
        context.m_gdt->WriteEntry(gdt_group::MATERIALS, entry);
    }
} // namespace material
