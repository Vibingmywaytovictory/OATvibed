#include "MenuListDumperIW3.h"

#include "MenuWriterIW3.h"

#include "Game/IW3/CommonIW3.h"

#include <filesystem>
#include <format>

namespace fs = std::filesystem;

using namespace IW3;

namespace
{
    constexpr auto MENU_FILE_EXTENSION = ".menu";

    // The menu parser reads a menu file path into a 64 byte buffer, so 63 characters is the longest it can open.
    // Measured against linker_pc: 63 links, 64 gives "Couldn't find menu file".
    constexpr auto MAX_MENU_FILE_PATH_LENGTH = 63uz;

    void DumpMenus(menu::IWriterIW3& menuDumper, menu::MenuDumpingZoneState* zoneState, const MenuList* menuList)
    {
        if (!menuList->menus)
            return;

        for (auto menuNum = 0; menuNum < menuList->menuCount; menuNum++)
        {
            const auto* menu = menuList->menus[menuNum];
            if (!menu)
                continue;

            const auto menuDumpingState = zoneState->m_menu_dumping_state_map.find(menu);
            if (menuDumpingState == zoneState->m_menu_dumping_state_map.end())
                continue;

            // If the menu was embedded directly as menu list write its data in the menu list file
            if (menuDumpingState->second.m_alias_menu_list == menuList)
                menuDumper.WriteMenu(*menu);
            else
                menuDumper.IncludeMenu(menuDumpingState->second.m_path);
        }
    }

    std::string PathForMenu(const std::string& menuListParentPath, const menuDef_t* menu)
    {
        return menu::MenuFilePathIW3(menuListParentPath, menu->window.name);
    }
} // namespace

namespace menu
{
    std::string MenuFilePathIW3(const std::string& parentPath, const char* menuName)
    {
        if (!menuName)
            return {};

        // A menu of another fastfile is stored under its own name, the comma only marks it as a reference
        if (menuName[0] == ',')
            menuName++;

        auto path = std::format("{}{}{}", parentPath, menuName, MENU_FILE_EXTENSION);
        if (path.size() <= MAX_MENU_FILE_PATH_LENGTH)
            return path;

        // The parser reads the path of a loadMenu into a fixed buffer and silently fails to open a longer one,
        // so an overlong name has to be shortened. Only the file name is affected: a menu carries its real name
        // inside the file and that is what becomes the asset name. The hash keeps two names that shorten to the
        // same prefix apart.
        const auto suffix = std::format("_{:08x}{}", IW3::Common::R_HashString(menuName), MENU_FILE_EXTENSION);
        if (parentPath.size() + suffix.size() >= MAX_MENU_FILE_PATH_LENGTH)
            return path;

        const auto roomForName = MAX_MENU_FILE_PATH_LENGTH - parentPath.size() - suffix.size();

        return std::format("{}{}{}", parentPath, std::string_view(menuName).substr(0, roomForName), suffix);
    }

    void CreateDumpingStateForMenuListIW3(MenuDumpingZoneState* zoneState, const MenuList* menuList)
    {
        if (!menuList || menuList->menuCount <= 0 || !menuList->menus || !menuList->name)
            return;

        const std::string menuListName(menuList->name);
        const fs::path p(menuListName);
        std::string parentPath;
        if (p.has_parent_path())
            parentPath = p.parent_path().generic_string() + "/";

        for (auto i = 0; i < menuList->menuCount; i++)
        {
            const auto* menu = menuList->menus[i];

            if (!menu)
                continue;

            auto menuPath = PathForMenu(parentPath, menu);
            if (menuPath.empty())
                continue;

            auto existingState = zoneState->m_menu_dumping_state_map.find(menu);
            if (existingState == zoneState->m_menu_dumping_state_map.end())
            {
                const auto isTheSameAsMenuList = menuPath == menuListName;
                zoneState->CreateMenuDumpingState(menu, std::move(menuPath), isTheSameAsMenuList ? menuList : nullptr);
            }
            else if (!existingState->second.m_alias_menu_list)
            {
                const auto isTheSameAsMenuList = menuPath == menuListName;
                if (isTheSameAsMenuList)
                {
                    existingState->second.m_alias_menu_list = menuList;
                    existingState->second.m_path = std::move(menuPath);
                }
            }
        }
    }

    void MenuListDumperIW3::DumpAsset(AssetDumpingContext& context, const XAssetInfo<AssetMenuList::Type>& asset)
    {
        const auto* menuList = asset.Asset();
        const auto assetFile = context.OpenAssetFile(asset.m_name);

        if (!assetFile)
            return;

        auto* zoneState = context.GetZoneAssetDumperState<MenuDumpingZoneState>();

        const auto menuWriter = CreateMenuWriterIW3(*assetFile);

        menuWriter->Start();
        DumpMenus(*menuWriter, zoneState, menuList);
        menuWriter->End();
    }

    void MenuListDumperIW3::Dump(AssetDumpingContext& context)
    {
        auto* zoneState = context.GetZoneAssetDumperState<MenuDumpingZoneState>();

        const auto menuListAssets = context.m_zone.m_pools.PoolAssets<AssetMenuList>();
        for (const auto* asset : menuListAssets)
            CreateDumpingStateForMenuListIW3(zoneState, asset->Asset());

        AbstractAssetDumper::Dump(context);
    }
} // namespace menu
