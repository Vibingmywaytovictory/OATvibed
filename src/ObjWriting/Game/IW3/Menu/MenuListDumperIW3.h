#pragma once

#include "Dumping/AbstractAssetDumper.h"
#include "Game/IW3/IW3.h"
#include "Menu/MenuDumpingZoneState.h"

#include <string>

namespace menu
{
    /**
     * \brief Builds the path of the file a menu is dumped to, short enough for the game's menu parser to open it.
     *
     * \param parentPath Folder the menu belongs in, with a trailing slash, or empty for the dump root.
     * \param menuName Name of the menu asset. A leading comma, marking a menu of another fastfile, is dropped.
     */
    std::string MenuFilePathIW3(const std::string& parentPath, const char* menuName);

    void CreateDumpingStateForMenuListIW3(MenuDumpingZoneState* zoneState, const IW3::MenuList* menuList);

    class MenuListDumperIW3 final : public AbstractAssetDumper<IW3::AssetMenuList>
    {
    public:
        void Dump(AssetDumpingContext& context) override;

    protected:
        void DumpAsset(AssetDumpingContext& context, const XAssetInfo<IW3::AssetMenuList::Type>& asset) override;
    };
} // namespace menu
