#include "MaterialGdtZoneState.h"

namespace material
{
    void GdtMaterials::Add(std::string materialName)
    {
        m_written.emplace(std::move(materialName));
    }

    bool GdtMaterials::Contains(const std::string& materialName) const
    {
        return m_written.contains(materialName);
    }
} // namespace material
