#include "WeaponGdtLoaderIW3.h"

#include "Game/IW3/IW3.h"
#include "Game/IW3/ObjConstantsIW3.h"
#include "InfoString/InfoString.h"
#include "Utils/Logging/Log.h"
#include "WeaponInfoStringLoaderIW3.h"

#include <cstring>
#include <format>

using namespace IW3;

namespace
{
    class GdtLoaderWeapon final : public AssetCreator<AssetWeapon>
    {
    public:
        GdtLoaderWeapon(MemoryManager& memory, ISearchPath& searchPath, IGdtQueryable& gdt, Zone& zone)
            : m_gdt(gdt),
              m_info_string_loader(memory, searchPath, zone)
        {
        }

        AssetCreationResult CreateAsset(const std::string& assetName, AssetCreationContext& context) override
        {
            // CoD4 splits weapons over one gdf per weapType, so the entry may live in any of them
            const GdtEntry* gdtEntry = nullptr;
            for (const auto* gdfName : {GDF_FILENAME_WEAPON_BULLET, GDF_FILENAME_WEAPON_GRENADE, GDF_FILENAME_WEAPON_PROJECTILE})
            {
                gdtEntry = m_gdt.GetGdtEntryByGdfAndName(gdfName, assetName);
                if (gdtEntry)
                    break;
            }

            if (gdtEntry == nullptr)
                return AssetCreationResult::NoAction();

            InfoString infoString;
            if (!infoString.FromGdtProperties(*gdtEntry))
            {
                con::error("Failed to read weapon gdt entry: \"{}\"", assetName);
                return AssetCreationResult::Failure();
            }

            return m_info_string_loader.CreateAsset(assetName, infoString, context);
        }

    private:
        IGdtQueryable& m_gdt;
        weapon::InfoStringLoaderIW3 m_info_string_loader;
    };
} // namespace

namespace weapon
{
    std::unique_ptr<AssetCreator<AssetWeapon>> CreateGdtLoaderIW3(MemoryManager& memory, ISearchPath& searchPath, IGdtQueryable& gdt, Zone& zone)
    {
        return std::make_unique<GdtLoaderWeapon>(memory, searchPath, gdt, zone);
    }
} // namespace weapon
