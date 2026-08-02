#include "MaterialGdtZoneState.h"

namespace material
{
    bool GdtMaterials::Add(std::string materialName)
    {
        return m_written.emplace(std::move(materialName)).second;
    }

    bool GdtMaterials::Contains(const std::string& materialName) const
    {
        return m_written.contains(materialName);
    }
} // namespace material
