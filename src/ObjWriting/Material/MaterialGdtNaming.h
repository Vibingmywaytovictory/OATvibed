#pragma once

namespace material
{
    /**
     * \brief Strips the technique-category prefix (mc/, wc/) off a material asset name.
     *
     * The native tools never see prefixed names: AssetManager's converter derives the prefix from the entry's
     * materialType and prepends it to the entry name, and the linker strips the first path component when opening
     * raw/materials. A gdt entry or model export that carries the prefix therefore converts into files the linker
     * cannot find, named mc/mc/<name>. Stock authoring keeps one flat file per stripped name, which mc/ and wc/
     * variants both compile from.
     */
    [[nodiscard]] const char* StripTechniqueCategoryPrefix(const char* name);
} // namespace material
