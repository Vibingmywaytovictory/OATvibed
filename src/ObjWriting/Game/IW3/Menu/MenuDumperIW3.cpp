#include "MenuDumperIW3.h"

#include "MenuListDumperIW3.h"
#include "MenuWriterIW3.h"
#include "ObjWriting.h"

#include <format>
#include <string>

using namespace IW3;

namespace
{
    std::string GetPathForMenu(menu::MenuDumpingZoneState* zoneState, const XAssetInfo<menuDef_t>& asset)
    {
        const auto menuDumpingState = zoneState->m_menu_dumping_state_map.find(asset.Asset());

        // A menu belonging to no list still has to land somewhere the parser can open again
        if (menuDumpingState == zoneState->m_menu_dumping_state_map.end())
            return menu::MenuFilePathIW3("ui_mp/", asset.Asset()->window.name);

        return menuDumpingState->second.m_path;
    }
} // namespace

namespace menu
{
    void MenuDumperIW3::DumpAsset(AssetDumpingContext& context, const XAssetInfo<AssetMenu::Type>& asset)
    {
        const auto* menu = asset.Asset();
        auto* zoneState = context.GetZoneAssetDumperState<MenuDumpingZoneState>();

        if (!ObjWriting::ShouldHandleAssetType(ASSET_TYPE_MENULIST))
        {
            // Make sure menu paths based on menu lists are created
            const auto menuListAssets = context.m_zone.m_pools.PoolAssets<AssetMenuList>();
            for (auto* menuListAsset : menuListAssets)
                CreateDumpingStateForMenuListIW3(zoneState, menuListAsset->Asset());
        }
        else
        {
            // A menu that shares its path with the list it belongs to is written into that list's file by the menu
            // list dumper. Writing it here as well would truncate the list file and drop every other member.
            const auto menuDumpingState = zoneState->m_menu_dumping_state_map.find(asset.Asset());
            if (menuDumpingState != zoneState->m_menu_dumping_state_map.end() && menuDumpingState->second.m_alias_menu_list)
                return;
        }

        const auto menuFilePath = GetPathForMenu(zoneState, asset);
        const auto assetFile = context.OpenAssetFile(menuFilePath);

        if (!assetFile)
            return;

        const auto menuWriter = CreateMenuWriterIW3(*assetFile);

        menuWriter->Start();
        menuWriter->WriteMenu(*menu);
        menuWriter->End();
    }
} // namespace menu
