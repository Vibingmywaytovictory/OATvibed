#pragma once

#include "Dumping/AbstractAssetDumper.h"
#include "Game/IW3/IW3.h"

namespace sound
{
    /**
     * \brief Dumps the sound aliases of a zone as one soundaliases csv.
     *
     * The csv uses the stock column set, so the native linker compiles it via a `sound,<zone>` zone source entry.
     * File paths reference the sounds the way LoadedSoundDumperIW3 dumps them (relative to sound/).
     */
    class AliasDumperIW3 final : public AbstractSingleProgressAssetDumper<IW3::AssetSound>
    {
    public:
        void Dump(AssetDumpingContext& context) override;
    };
} // namespace sound
