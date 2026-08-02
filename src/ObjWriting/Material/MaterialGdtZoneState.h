#pragma once

#include "Dumping/IZoneAssetDumperState.h"

#include <string>
#include <unordered_set>

namespace material
{
    /**
     * \brief Records the materials that made it into the gdt.
     *
     * AssetManager aborts an entire "build all" pass on the first asset that fails, so anything depending on a
     * material that is not in the gdt has to be left out as well. Tracking what was written rather than what was
     * skipped also covers materials that never reach a dumper at all, such as references to another fastfile.
     * Material dumpers run before xmodel dumpers so that this is filled in by the time it is read.
     */
    class GdtMaterials final : public IZoneAssetDumperState
    {
    public:
        void Add(std::string materialName);
        [[nodiscard]] bool Contains(const std::string& materialName) const;

    private:
        std::unordered_set<std::string> m_written;
    };
} // namespace material
