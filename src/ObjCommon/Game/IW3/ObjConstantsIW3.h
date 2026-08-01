#pragma once

namespace IW3
{
    static constexpr auto INFO_STRING_PREFIX_PHYS_PRESET = "PHYSIC";
    static constexpr auto INFO_STRING_PREFIX_WEAPON = "WEAPONFILE";

    static constexpr auto GDF_FILENAME_MATERIAL = "material.gdf";
    static constexpr auto GDF_FILENAME_PHYS_PRESET = "physpreset.gdf";
    static constexpr auto GDF_FILENAME_XMODEL = "xmodel.gdf";

    // There is no weapon.gdf in CoD4. AssetManager splits weapons over one gdf per weapType and rejects
    // the whole gdt when an entry names a gdf that does not exist.
    static constexpr auto GDF_FILENAME_WEAPON_BULLET = "bulletweapon.gdf";
    static constexpr auto GDF_FILENAME_WEAPON_GRENADE = "grenadeweapon.gdf";
    static constexpr auto GDF_FILENAME_WEAPON_PROJECTILE = "projectileweapon.gdf";
} // namespace IW3
