#include "Game/IW3/Material/PackedSpecularImageIW3.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

using namespace std::literals;

namespace
{
    TEST_CASE("PackedSpecularImage: Reads both source names out of the packed name", "[iw3][material]")
    {
        const auto sources = material::GetPackedSpecularSourceNames("~cobra_spc-rgb&cobra_cos-l-11");

        CHECK(sources.m_spec_color_map == "cobra_spc"s);
        CHECK(sources.m_cosine_power_map == "cobra_cos"s);
    }

    TEST_CASE("PackedSpecularImage: Names the cosine power map after a shared source", "[iw3][material]")
    {
        // One authored image supplied the color and, through its luminance, the cosine power
        const auto sources = material::GetPackedSpecularSourceNames("~weapon_ak5_stock_spc-rgbl-11");

        CHECK(sources.m_spec_color_map == "weapon_ak5_stock_spc"s);
        CHECK(sources.m_cosine_power_map == "weapon_ak5_stock_spc_cos"s);
    }

    TEST_CASE("PackedSpecularImage: Leaves out maps the material did not have", "[iw3][material]")
    {
        // A built-in stands in for the map that was not set
        const auto withoutCosinePower = material::GetPackedSpecularSourceNames("~hellfire_missile_spc-rgb&$white-l-11");
        CHECK(withoutCosinePower.m_spec_color_map == "hellfire_missile_spc"s);
        CHECK(withoutCosinePower.m_cosine_power_map.empty());

        const auto withoutSpecColor = material::GetPackedSpecularSourceNames("~$white-rgb&eye_cos-l");
        CHECK(withoutSpecColor.m_spec_color_map.empty());
        CHECK(withoutSpecColor.m_cosine_power_map == "eye_cos"s);

        const auto withoutAnything = material::GetPackedSpecularSourceNames("~$white-rgb&$white-l-11");
        CHECK(withoutAnything.m_spec_color_map.empty());
        CHECK(withoutAnything.m_cosine_power_map.empty());
    }

    TEST_CASE("PackedSpecularImage: Falls back to the packed name when nothing is recoverable", "[iw3][material]")
    {
        // A name too long for the converter is cut short and hashed
        const auto truncated = material::GetPackedSpecularSourceNames("~plr_mw2_ranger_smg_alpha_spc~59a89730");
        CHECK(truncated.m_spec_color_map == "plr_mw2_ranger_smg_alpha_spc_59a89730"s);

        // A material compiled with a specular strength other than the default has it baked into the name, and the
        // pixels of the packed image are already scaled by it
        const auto scaled = material::GetPackedSpecularSourceNames("~flag_neutral_spc-r-17g-17b-17l-11");
        CHECK(scaled.m_spec_color_map == "flag_neutral_spc_r_17g_17b_17l_11"s);
        CHECK(scaled.m_cosine_power_map == "flag_neutral_spc_r_17g_17b_17l_11_cos"s);
    }

    TEST_CASE("PackedSpecularImage: Keeps names short enough for AssetManager", "[iw3][material]")
    {
        // AssetManager refuses a source image whose iwi file name is longer than 42 characters
        constexpr auto maxNameLength = 42uz - 4uz;

        const auto sources = material::GetPackedSpecularSourceNames("~plr_mw2_ranger_smg_alpha_spc~59a89730");
        CHECK(sources.m_cosine_power_map.size() <= maxNameLength);
        CHECK(sources.m_cosine_power_map.starts_with("plr_mw2_ranger_smg_alpha_spc_"s));

        // Two names that only differ past the cut still come out apart
        const auto other = material::GetPackedSpecularSourceNames("~plr_mw2_ranger_smg_alpha_spc~1234abcd");
        CHECK(other.m_cosine_power_map.size() <= maxNameLength);
        CHECK(other.m_cosine_power_map != sources.m_cosine_power_map);
    }

    TEST_CASE("PackedSpecularImage: Undoes the cosine power scale for every packed value", "[iw3][material]")
    {
        CHECK(material::UnscaleCosinePower(0u) == 0u);
        CHECK(material::UnscaleCosinePower(122u) == 128u);
        CHECK(material::UnscaleCosinePower(243u) == 255u);

        // Compositing the unscaled value has to give the packed value back
        for (auto packedValue = 0u; packedValue <= 243u; packedValue++)
        {
            const auto unscaled = static_cast<unsigned>(material::UnscaleCosinePower(static_cast<std::uint8_t>(packedValue)));
            CHECK((unscaled * 61u + 32u) / 64u == packedValue);
        }

        // Values above what compositing can produce, which only block compression noise reaches, clamp
        CHECK(material::UnscaleCosinePower(255u) == 255u);
    }
} // namespace
