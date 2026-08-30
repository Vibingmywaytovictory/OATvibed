#pragma once

#include "Game/IW3/IW3.h"

#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace IW3
{
    inline const char* surfaceTypeNames[]{
        "default", "bark",   "brick",   "carpet",  "cloth",   "concrete", "dirt",    "flesh", "foliage",      "glass",
        "grass",   "gravel", "ice",     "metal",   "mud",     "paper",    "plaster", "rock",  "sand",         "snow",
        "water",   "wood",   "asphalt", "ceramic", "plastic", "rubber",   "cushion", "fruit", "paintedmetal",
    };
    static_assert(std::extent_v<decltype(surfaceTypeNames)> == SURF_TYPE_NUM);

    // ==========================================================================
    // GDT values, as defined by deffiles/material.gdf of the CoD4 mod tools.
    //
    // AssetManager writes enum values verbatim, including the trailing asterisk
    // that marks the default of an enum, so these strings keep it.
    // ==========================================================================

    constexpr auto GDT_MATERIAL_TEMPLATE = "material.template";

    enum class GdtMaterialType_e
    {
        UNKNOWN,
        WORLD_PHONG,
        MODEL_PHONG,
        MODEL_AMBIENT,
        WORLD_UNLIT,
        MODEL_UNLIT,
        UNLIT,
        TWO_D,
        IMPACT_MARK,
        EFFECT,
        DISTORTION,
        PARTICLE_CLOUD,
        TOOLS,
        SKY,
        WATER,
        OBJECTIVE,
        CUSTOM,

        COUNT
    };

    inline const char* GdtMaterialTypeNames[]{
        "",
        "world phong",
        "model phong",
        "model ambient",
        "world unlit",
        "model unlit",
        "unlit",
        "2d",
        "impact mark",
        "effect",
        "distortion",
        "particle cloud",
        "tools",
        "sky",
        "water",
        "objective",
        "custom",
    };
    static_assert(std::extent_v<decltype(GdtMaterialTypeNames)> == static_cast<size_t>(GdtMaterialType_e::COUNT));

    // The gdf offers a single value only, unlike the surfaceTypeBits bitfield of the compiled material
    constexpr auto GDT_SURFACE_TYPE_NONE = "<none>";

    // Left at the default, AssetManager derives the sort from the blend func and material type, which reproduces the
    // original sort key for the great majority of materials but not for the ones that were sorted by hand.
    constexpr auto GDT_SORT_DEFAULT = "<default>*";

    /**
     * rief The sort key each gdf "sort" value compiles to, measured by converting one material once per value.
     *
     * The gdf list is not contiguous - the engine keeps gaps between the groups - so the sort key cannot index it and
     * a material whose key falls in a gap has to stay at the default.
     */
    inline const std::unordered_map<unsigned char, const char*> GdtSortNames{
        {0,  "distortion"             },
        {1,  "opaque water"           },
        {2,  "boat hull"              },
        {3,  "opaque ambient"         },
        {4,  "opaque"                 },
        {5,  "sky"                    },
        {6,  "skybox - sun / moon"    },
        {7,  "skybox - clouds"        },
        {8,  "skybox - horizon"       },
        {9,  "decal - bottom 1"       },
        {10, "decal - bottom 2"       },
        {11, "decal - bottom 3"       },
        {12, "decal - static decal"   },
        {13, "decal - middle 1"       },
        {14, "decal - middle 2"       },
        {15, "decal - middle 3"       },
        {24, "decal - weapon impact"  },
        {29, "decal - top 1"          },
        {30, "decal - top 2"          },
        {31, "decal - top 3"          },
        {32, "multiplicative"         },
        {33, "banner / curtain"       },
        {34, "hair"                   },
        {35, "underwater"             },
        {36, "transparent water"      },
        {37, "corona"                 },
        {38, "window inside"          },
        {39, "window outside"         },
        {40, "before effects - bottom"},
        {41, "before effects - middle"},
        {42, "before effects - top"   },
        {43, "blend / additive"       },
        {48, "effect - auto sort"     },
        {56, "after effects - bottom" },
        {57, "after effects - middle" },
        {58, "after effects - top"    },
        {59, "viewmodel effect"       },
    };

    constexpr auto GDT_USAGE_NOT_IN_EDITOR = "<not in editor>";

    enum class GdtBlendFunc_e
    {
        REPLACE,
        BLEND,
        ADD,
        MULTIPLY,
        SCREEN_ADD,
        CUSTOM,

        COUNT
    };

    inline const char* GdtBlendFuncNames[]{
        "Replace*",
        "Blend",
        "Add",
        "Multiply",
        "Screen Add",
        "Custom",
    };
    static_assert(std::extent_v<decltype(GdtBlendFuncNames)> == static_cast<size_t>(GdtBlendFunc_e::COUNT));

    // Indexed by GfxBlendOp
    inline const char* GdtBlendOpNames[]{
        "Disable",
        "Add*",
        "Subtract",
        "RevSubtract",
        "Min",
        "Max",
    };

    // Indexed by GfxBlend
    inline const char* GdtCustomBlendFuncNames[]{
        "",
        "Zero",
        "One",
        "SrcColor",
        "InvSrcColor",
        "SrcAlpha",
        "InvSrcAlpha",
        "DestAlpha",
        "InvDestAlpha",
        "DestColor",
        "InvDestColor",
    };

    // Indexed by GfxAlphaTest
    constexpr auto GDT_ALPHA_TEST_ALWAYS = "Always*";
    constexpr auto GDT_ALPHA_TEST_GE128 = "GE128";

    constexpr auto GDT_DEPTH_TEST_LESS_EQUAL = "LessEqual*";
    constexpr auto GDT_DEPTH_TEST_LESS = "Less";
    constexpr auto GDT_DEPTH_TEST_EQUAL = "Equal";
    constexpr auto GDT_DEPTH_TEST_ALWAYS = "Always";
    constexpr auto GDT_DEPTH_TEST_DISABLE = "Disable";

    constexpr auto GDT_DEPTH_WRITE_AUTO = "<auto>*";
    constexpr auto GDT_DEPTH_WRITE_ON = "On";
    constexpr auto GDT_DEPTH_WRITE_OFF = "Off";

    // Indexed by GfxCullFace
    inline const char* GdtCullFaceNames[]{
        "None",
        "None",
        "Back*",
        "Front",
    };

    // Indexed by the polygonOffset state bits
    inline const char* GdtPolygonOffsetNames[]{
        "None*",
        "Static Decal",
        "Weapon Impact",
        "None*",
    };

    constexpr auto GDT_ENABLE = "Enable";
    constexpr auto GDT_DISABLE = "Disable";

    enum class GdtTileMode_e
    {
        TILE_BOTH,
        TILE_HORIZONTAL,
        TILE_VERTICAL,
        NO_TILE,

        COUNT
    };

    inline const char* GdtTileModeNames[]{
        "tile both*",
        "tile horizontal",
        "tile vertical",
        "no tile",
    };
    static_assert(std::extent_v<decltype(GdtTileModeNames)> == static_cast<size_t>(GdtTileMode_e::COUNT));

    enum class GdtFilter_e
    {
        MIP_2X_BILINEAR,
        AUTO,
        BILINEAR,
        TRILINEAR,
        ANISOTROPIC,
        MIP_4X_BILINEAR,
        MIP_2X_TRILINEAR,
        MIP_4X_TRILINEAR,
        NOMIP_NEAREST,
        NEAREST,
        NOMIP_BILINEAR,
        LINEAR,

        COUNT
    };

    inline const char* GdtFilterNames[]{
        "mip standard (2x bilinear)*",
        "<auto filter>*",
        "bilinear",
        "trilinear",
        "anisotropic",
        "mip expensive (4x bilinear)",
        "mip more expensive (2x trilinear)",
        "mip most expensive (4x trilinear)",
        "nomip nearest",
        "nearest",
        "nomip bilinear",
        "linear",
    };
    static_assert(std::extent_v<decltype(GdtFilterNames)> == static_cast<size_t>(GdtFilter_e::COUNT));

    constexpr auto GDT_FORMAT_AUTO = "<auto compression>*";

    // Every field of material.gdf with the default AssetManager itself would write.
    // Generated from deffiles/material.gdf (#version 98) of the CoD4 mod tools.
    // Values a compiled material actually carries are overwritten by the dumper.
    inline const std::pair<const char*, const char*> gdtMaterialDefaults[]{
        {"aiClip",                        "0"                          },
        {"aiSightClip",                   "0"                          },
        {"alphaTest",                     "Always*"                    },
        {"blendFunc",                     "Replace*"                   },
        {"bulletClip",                    "0"                          },
        {"canShootClip",                  "0"                          },
        {"colorMap",                      ""                           },
        {"colorWriteAlpha",               "Enable"                     },
        {"colorWriteBlue",                "Enable"                     },
        {"colorWriteGreen",               "Enable"                     },
        {"colorWriteRed",                 "Enable"                     },
        {"cosinePowerMap",                ""                           },
        {"cosinePowerStrength",           "100"                        },
        {"cullFace",                      "Back*"                      },
        {"customBlendOpAlpha",            "Add*"                       },
        {"customBlendOpRgb",              "Add*"                       },
        {"customString",                  ""                           },
        {"customTemplate",                ""                           },
        {"depthTest",                     "LessEqual*"                 },
        {"depthWrite",                    "<auto>*"                    },
        {"destCustomBlendFunc",           "One*"                       },
        {"destCustomBlendFuncAlpha",      "One*"                       },
        {"detail",                        "0"                          },
        {"detailMap",                     ""                           },
        {"detailScaleX",                  "8"                          },
        {"detailScaleY",                  "8"                          },
        {"distFalloff",                   "0"                          },
        {"distFalloffBeginDistance",      "200.0"                      },
        {"distFalloffEndDistance",        "10.0"                       },
        {"distortionColorBehavior",       "scales distortion strength*"},
        {"distortionScaleX",              "0.5"                        },
        {"distortionScaleY",              "0.5"                        },
        {"drawToggle",                    "0"                          },
        {"envMapExponent",                "2.5"                        },
        {"envMapMax",                     "1.0"                        },
        {"envMapMin",                     "0.2"                        },
        {"eyeOffsetDepth",                "0"                          },
        {"falloff",                       "0"                          },
        {"falloffBeginAngle",             "35.0"                       },
        {"falloffEndAngle",               "65.0"                       },
        {"filterColor",                   "mip standard (2x bilinear)*"},
        {"filterDetail",                  "mip standard (2x bilinear)*"},
        {"filterNormal",                  "mip standard (2x bilinear)*"},
        {"filterSpecular",                "mip standard (2x bilinear)*"},
        {"formatColor",                   "<auto compression>*"        },
        {"formatDetail",                  "<auto compression>*"        },
        {"formatNormal",                  "<auto compression>*"        },
        {"formatSpecular",                "<auto compression>*"        },
        {"hasEditorMaterial",             "0"                          },
        {"hdrPortal",                     "0"                          },
        {"itemClip",                      "0"                          },
        {"ladder",                        "0"                          },
        {"lightPortal",                   "0"                          },
        {"locale_Chechnya",               "0"                          },
        {"locale_Duhoc",                  "0"                          },
        {"locale_Egypt",                  "0"                          },
        {"locale_Generic",                "0"                          },
        {"locale_Industrial",             "0"                          },
        {"locale_Kiev",                   "0"                          },
        {"locale_Libya",                  "0"                          },
        {"locale_London",                 "0"                          },
        {"locale_Middle_East",            "0"                          },
        {"locale_Modern_America",         "0"                          },
        {"locale_Poland",                 "0"                          },
        {"locale_Tunisia",                "0"                          },
        {"locale_Villers",                "0"                          },
        {"locale_case",                   "0"                          },
        {"locale_decal",                  "0"                          },
        {"locale_test",                   "0"                          },
        {"locale_tools",                  "0"                          },
        {"mantleOn",                      "0"                          },
        {"mantleOver",                    "0"                          },
        {"materialType",                  "world phong"                },
        {"missileClip",                   "0"                          },
        {"noCastShadow",                  "0"                          },
        {"noDraw",                        "0"                          },
        {"noDrop",                        "0"                          },
        {"noDynamicLight",                "0"                          },
        {"noFallDamage",                  "0"                          },
        {"noFog",                         "0"                          },
        {"noImpact",                      "0"                          },
        {"noLightmap",                    "0"                          },
        {"noMarks",                       "0"                          },
        {"noPenetrate",                   "0"                          },
        {"noReceiveDynamicShadow",        "0"                          },
        {"noSteps",                       "0"                          },
        {"noStreamColor",                 "0"                          },
        {"nonColliding",                  "0"                          },
        {"nonSolid",                      "0"                          },
        {"nopicmipColor",                 "0"                          },
        {"nopicmipDetail",                "0"                          },
        {"nopicmipNormal",                "0"                          },
        {"nopicmipSpecular",              "0"                          },
        {"normalMap",                     ""                           },
        {"origin",                        "0"                          },
        {"outdoorOnly",                   "0"                          },
        {"physicsGeom",                   "0"                          },
        {"playerClip",                    "0"                          },
        {"polygonOffset",                 "None*"                      },
        {"portal",                        "0"                          },
        {"radialNormals",                 "0"                          },
        {"showAdvancedOptions",           "<none>*"                    },
        {"sky",                           "0"                          },
        {"slick",                         "0"                          },
        {"sort",                          "<default>*"                 },
        {"specColorMap",                  ""                           },
        {"specColorStrength",             "100"                        },
        {"srcCustomBlendFunc",            "One*"                       },
        {"srcCustomBlendFuncAlpha",       "One*"                       },
        {"stencil",                       "Disable"                    },
        {"stencilFunc1",                  "Always"                     },
        {"stencilFunc2",                  "Always"                     },
        {"stencilOpFail1",                "Keep"                       },
        {"stencilOpFail2",                "Keep"                       },
        {"stencilOpPass1",                "Keep"                       },
        {"stencilOpPass2",                "Keep"                       },
        {"stencilOpZFail1",               "Keep"                       },
        {"stencilOpZFail2",               "Keep"                       },
        {"structural",                    "0"                          },
        {"surfaceType",                   "<error>"                    },
        {"template",                      "material.template"          },
        {"tessSize",                      "0"                          },
        {"texScroll",                     "0"                          },
        {"textureAtlasColumnCount",       "1"                          },
        {"textureAtlasRowCount",          "1"                          },
        {"tileColor",                     "tile both*"                 },
        {"tileNormal",                    "tile both*"                 },
        {"tileSpecular",                  "tile both*"                 },
        {"transparent",                   "0"                          },
        {"usage",                         "<not in editor>"            },
        {"useLegacyNormalEncoding",       "0"                          },
        {"useSpotLight",                  "0"                          },
        {"vehicleClip",                   "0"                          },
        {"waterColorB",                   "0.4"                        },
        {"waterColorG",                   "0.3"                        },
        {"waterColorR",                   "0.2"                        },
        {"waterMapAmplitude",             "0.06"                       },
        {"waterMapHorizontalWorldLength", "37"                         },
        {"waterMapTextureWidth",          "64"                         },
        {"waterMapVerticalWorldLength",   "37"                         },
        {"waterMapWindDirectionX",        "1"                          },
        {"waterMapWindDirectionY",        "0"                          },
        {"waterMapWindSpeed",             "76"                         },
        {"zFeather",                      "0"                          },
        {"zFeatherDepth",                 "40"                         },
    };

    struct GdtMaterialTextureMap
    {
        const char* m_gdt_name;
        const char* m_property_suffix;
    };

    // Maps the sampler name of the compiled material onto the gdf field that produced it.
    // CoD4 packs specular color and cosine power into one map, so cosinePowerMap cannot be recovered.
    inline const std::unordered_map<std::string_view, GdtMaterialTextureMap> gdtMaterialTextureMaps{
        {"colorMap",    {"colorMap", "Color"}       },
        {"detailMap",   {"detailMap", "Detail"}     },
        {"normalMap",   {"normalMap", "Normal"}     },
        {"specularMap", {"specColorMap", "Specular"}},
    };
} // namespace IW3
