#include "Material/MaterialGdtNaming.h"
#include "Material/MaterialGdtZoneState.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace std::literals;

namespace
{
    TEST_CASE("MaterialGdtNaming: Strips technique category prefixes", "[material]")
    {
        CHECK(material::StripTechniqueCategoryPrefix("mc/mtl_weapon_ak47") == "mtl_weapon_ak47"s);
        CHECK(material::StripTechniqueCategoryPrefix("wc/gfx_impact_metal01") == "gfx_impact_metal01"s);
    }

    TEST_CASE("MaterialGdtNaming: Leaves unprefixed names untouched", "[material]")
    {
        CHECK(material::StripTechniqueCategoryPrefix("mtl_mw2_airborne_glove") == "mtl_mw2_airborne_glove"s);
        CHECK(material::StripTechniqueCategoryPrefix("white") == "white"s);
        // Only the technique category counts as a prefix, not arbitrary path components
        CHECK(material::StripTechniqueCategoryPrefix("gfx/fx_generic") == "gfx/fx_generic"s);
        CHECK(material::StripTechniqueCategoryPrefix(nullptr) == nullptr);
    }

    TEST_CASE("GdtMaterials: Rejects a second entry of the same name", "[material]")
    {
        material::GdtMaterials materials;

        CHECK(materials.Add("gfx_impact_metal01"));
        CHECK_FALSE(materials.Add("gfx_impact_metal01"));
        CHECK(materials.Contains("gfx_impact_metal01"));
    }
} // namespace
