#pragma once

#include "Dumping/AbstractAssetDumper.h"
#include "Game/IW3/IW3.h"

namespace material
{
    class GdtDumperIW3 final : public AbstractAssetDumper<IW3::AssetMaterial>
    {
    protected:
        void DumpAsset(AssetDumpingContext& context, const XAssetInfo<IW3::AssetMaterial::Type>& asset) override;
    };
} // namespace material
