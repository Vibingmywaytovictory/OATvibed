#include "Game/IW3/Menu/MenuListDumperIW3.h"

#include "Game/IW3/Menu/MenuDumperIW3.h"
#include "SearchPath/MockOutputPath.h"
#include "SearchPath/MockSearchPath.h"

#include <catch2/catch_test_macros.hpp>
#include <iterator>
#include <memory>

using namespace IW3;

namespace
{
    TEST_CASE("MenuListDumperIW3: References member menus by path", "[iw3][menu][menulist][assetdumper]")
    {
        menuDef_t mainMenu{};
        mainMenu.window.name = "main";

        menuDef_t optionsMenu{};
        optionsMenu.window.name = "options";

        menuDef_t confirmationMenu{};
        confirmationMenu.window.name = ",confirmation";

        menuDef_t* menus[]{&mainMenu, &optionsMenu, &confirmationMenu};

        MenuList menuList{};
        menuList.name = "ui_mp/menus.txt";
        menuList.menuCount = static_cast<int>(std::size(menus));
        menuList.menus = menus;

        Zone zone("MockZone", 0, GameId::IW3, GamePlatform::PC);
        zone.m_pools.AddAsset(std::make_unique<XAssetInfo<MenuList>>(ASSET_TYPE_MENULIST, menuList.name, &menuList));

        MockSearchPath mockObjPath;
        MockOutputPath mockOutput;
        AssetDumpingContext context(zone, "", mockOutput, mockObjPath, std::nullopt);

        menu::MenuListDumperIW3 dumper;
        dumper.Dump(context);

        const auto* file = mockOutput.GetMockedFile("ui_mp/menus.txt");
        REQUIRE(file);

        // Every member keeps a file of its own. Inlining them into the list instead would blow linker_pc's 32768
        // byte cap on a menu file for any list the size of a stock one. A menu of another fastfile has nothing to
        // write and is only ever a reference.
        constexpr auto expectedOutput = R"({
    loadMenu { "ui_mp/main.menu" }
    loadMenu { "ui_mp/options.menu" }
    loadMenu { "ui_mp/confirmation.menu" }
}
)";
        REQUIRE(file->AsString() == expectedOutput);
    }

    TEST_CASE("MenuListDumperIW3: Writes the member that shares the list path into the list file", "[iw3][menu][menulist][assetdumper]")
    {
        // The list is named after one of its members, which is how the stock zones store a menu that is its own
        // list. That member has to be written into the list file: giving it a file of its own would land on the
        // list's path and truncate it, dropping every other member.
        menuDef_t aliasMenu{};
        aliasMenu.window.name = "settings_quick_dm";

        menuDef_t otherMenu{};
        otherMenu.window.name = "settings_quick_dm_time_limit";

        menuDef_t* menus[]{&aliasMenu, &otherMenu};

        MenuList menuList{};
        menuList.name = "ui_mp/settings_quick_dm.menu";
        menuList.menuCount = static_cast<int>(std::size(menus));
        menuList.menus = menus;

        Zone zone("MockZone", 0, GameId::IW3, GamePlatform::PC);
        zone.m_pools.AddAsset(std::make_unique<XAssetInfo<MenuList>>(ASSET_TYPE_MENULIST, menuList.name, &menuList));
        zone.m_pools.AddAsset(std::make_unique<XAssetInfo<menuDef_t>>(ASSET_TYPE_MENU, aliasMenu.window.name, &aliasMenu));
        zone.m_pools.AddAsset(std::make_unique<XAssetInfo<menuDef_t>>(ASSET_TYPE_MENU, otherMenu.window.name, &otherMenu));

        MockSearchPath mockObjPath;
        MockOutputPath mockOutput;
        AssetDumpingContext context(zone, "", mockOutput, mockObjPath, std::nullopt);

        menu::MenuListDumperIW3 listDumper;
        listDumper.Dump(context);
        menu::MenuDumperIW3 menuDumper;
        menuDumper.Dump(context);

        const auto* listFile = mockOutput.GetMockedFile("ui_mp/settings_quick_dm.menu");
        REQUIRE(listFile);

        constexpr auto expectedListOutput = R"({
    menuDef
    {
        name                        "settings_quick_dm"
        rect                        0 0 0 0 0 0
        forecolor                   0 0 0 0
    }
    loadMenu { "ui_mp/settings_quick_dm_time_limit.menu" }
}
)";
        // The menu dumper must not have written over the list
        REQUIRE(listFile->AsString() == expectedListOutput);

        // The member that does not share the list path still gets its own file
        REQUIRE(mockOutput.GetMockedFile("ui_mp/settings_quick_dm_time_limit.menu"));
    }
} // namespace
