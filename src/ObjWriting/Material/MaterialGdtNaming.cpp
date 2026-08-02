#include "MaterialGdtNaming.h"

#include <cstring>

namespace material
{
    const char* StripTechniqueCategoryPrefix(const char* name)
    {
        if (name && (std::strncmp(name, "mc/", 3) == 0 || std::strncmp(name, "wc/", 3) == 0))
            return &name[3];

        return name;
    }
} // namespace material
