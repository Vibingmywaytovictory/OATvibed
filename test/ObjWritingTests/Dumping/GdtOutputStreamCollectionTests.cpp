#include "Dumping/GdtOutputStreamCollection.h"
#include "SearchPath/MockOutputPath.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
    GdtEntry TestEntry(std::string name)
    {
        GdtEntry entry(std::move(name), "material.gdf");
        entry.m_properties["template"] = "material.template";

        return entry;
    }

    TEST_CASE("GdtOutputStreamCollection: Writes each group to a file of its own", "[gdt]")
    {
        MockOutputPath output;

        {
            GdtEntryNames entryNames;
            GdtOutputStreamCollection gdts(output, "mod", entryNames);
            gdts.WriteEntry(gdt_group::MATERIALS, TestEntry("mtl_weapon_ak47"));
            gdts.WriteEntry(gdt_group::XMODELS, TestEntry("viewmodel_ak47"));
        }

        const auto* materials = output.GetMockedFile("source_data/mod_materials.gdt");
        REQUIRE(materials != nullptr);
        CHECK(materials->AsString().find("\"mtl_weapon_ak47\"") != std::string::npos);
        CHECK(materials->AsString().find("\"viewmodel_ak47\"") == std::string::npos);

        const auto* xmodels = output.GetMockedFile("source_data/mod_xmodels.gdt");
        REQUIRE(xmodels != nullptr);
        CHECK(xmodels->AsString().find("\"viewmodel_ak47\"") != std::string::npos);
    }

    TEST_CASE("GdtOutputStreamCollection: Leaves out files of groups without entries", "[gdt]")
    {
        MockOutputPath output;

        {
            GdtEntryNames entryNames;
            GdtOutputStreamCollection gdts(output, "mod", entryNames);
            gdts.WriteEntry(gdt_group::MATERIALS, TestEntry("mtl_weapon_ak47"));
        }

        CHECK(output.GetMockedFile("source_data/mod_xmodels.gdt") == nullptr);
        // The zone definition points at the ungrouped file, so it is written even when nothing goes into it
        CHECK(output.GetMockedFile("source_data/mod.gdt") != nullptr);
    }

    TEST_CASE("GdtOutputStreamCollection: Writes no version entry", "[gdt]")
    {
        MockOutputPath output;

        {
            GdtEntryNames entryNames;
            GdtOutputStreamCollection gdts(output, "mod", entryNames);
            gdts.WriteEntry(TestEntry("weapon_ak47"));
            gdts.WriteEntry(gdt_group::MATERIALS, TestEntry("mtl_weapon_ak47"));
        }

        // AssetManager resolves every entry against a gdf of the same name and there is no version.gdf
        for (const auto& file : output.GetMockedFileList())
            CHECK(file.AsString().find("version.gdf") == std::string::npos);

        const auto* ungrouped = output.GetMockedFile("source_data/mod.gdt");
        REQUIRE(ungrouped != nullptr);
        CHECK(ungrouped->AsString().starts_with("{\n"));
        CHECK(ungrouped->AsString().ends_with("}"));
    }
    TEST_CASE("GdtOutputStreamCollection: Leaves out an entry name another zone already wrote", "[gdt]")
    {
        MockOutputPath output;
        // AssetManager refuses to convert at all while two gdts hold an entry of the same name, which is what
        // unlinking a mod together with the base game it overrides would otherwise produce
        GdtEntryNames entryNames;

        {
            GdtOutputStreamCollection modGdts(output, "mod", entryNames);
            modGdts.WriteEntry(gdt_group::MATERIALS, TestEntry("mtl_weapon_ak47"));
        }
        {
            GdtOutputStreamCollection commonGdts(output, "common_mp", entryNames);
            commonGdts.WriteEntry(gdt_group::MATERIALS, TestEntry("mtl_weapon_ak47"));
            commonGdts.WriteEntry(gdt_group::MATERIALS, TestEntry("mtl_weapon_m16"));
        }

        const auto* modMaterials = output.GetMockedFile("source_data/mod_materials.gdt");
        REQUIRE(modMaterials != nullptr);
        CHECK(modMaterials->AsString().find("\"mtl_weapon_ak47\"") != std::string::npos);

        const auto* commonMaterials = output.GetMockedFile("source_data/common_mp_materials.gdt");
        REQUIRE(commonMaterials != nullptr);
        CHECK(commonMaterials->AsString().find("\"mtl_weapon_ak47\"") == std::string::npos);
        CHECK(commonMaterials->AsString().find("\"mtl_weapon_m16\"") != std::string::npos);
    }
} // namespace
