#pragma once

#include "Obj/Gdt/GdtStream.h"
#include "SearchPath/IOutputPath.h"

#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace gdt_group
{
    // AssetManager aborts a whole "convert all" pass on the first entry that fails, so keeping an asset type in its
    // own file lets the user convert one type at a time and fix the failures of one before moving on to the next.
    constexpr auto MATERIALS = "materials";
    constexpr auto XMODELS = "xmodels";
    // Physpresets keep names as ordinary as "bottle_plastic", which the stock source_data/physicsettings.gdt already
    // uses. converter.exe refuses every conversion while two gdts it scans share a name, so a colliding physpreset
    // would otherwise take the weapons of the same file down with it when the user parks it.
    constexpr auto PHYS_PRESETS = "physpresets";
} // namespace gdt_group

/**
 * \brief The gdt entry names written so far, shared by every zone of one dump.
 *
 * AssetManager refuses to convert anything at all while two gdts under the folders it scans hold an entry of the same
 * name, and unlinking a mod together with the base game it overrides puts every asset the mod replaced into both
 * zones' gdts. The zone named first on the command line wins, which is the mod when it is named first.
 */
class GdtEntryNames
{
public:
    /**
     * \return False when an entry of that name was already written, so this one must be left out.
     */
    [[nodiscard]] bool Add(const std::string& entryName);

private:
    std::unordered_set<std::string> m_written;
};

/**
 * \brief The gdt files written for one zone, split by asset type group.
 *
 * A group file is only created when the first entry of its group is written, so a zone without assets of a group does
 * not leave an empty gdt behind. The ungrouped file - the one the zone definition points at - is always created.
 */
class GdtOutputStreamCollection
{
public:
    GdtOutputStreamCollection(IOutputPath& outputPath, std::string zoneName, GdtEntryNames& entryNames);
    ~GdtOutputStreamCollection();

    GdtOutputStreamCollection(const GdtOutputStreamCollection& other) = delete;
    GdtOutputStreamCollection(GdtOutputStreamCollection&& other) noexcept = delete;
    GdtOutputStreamCollection& operator=(const GdtOutputStreamCollection& other) = delete;
    GdtOutputStreamCollection& operator=(GdtOutputStreamCollection&& other) noexcept = delete;

    /**
     * \brief Writes an entry to the zone's ungrouped gdt file.
     */
    void WriteEntry(const GdtEntry& entry);

    /**
     * \brief Writes an entry to the gdt file of the specified group, creating it when it does not exist yet.
     */
    void WriteEntry(const std::string& group, const GdtEntry& entry);

private:
    struct OpenGdtFile
    {
        // The gdt stream keeps a reference to the file, so it must not outlive it
        std::unique_ptr<std::ostream> m_file;
        std::unique_ptr<GdtOutputStream> m_gdt;
    };

    GdtOutputStream* GetStreamForGroup(const std::string& group);

    IOutputPath& m_output_path;
    std::string m_zone_name;
    GdtEntryNames& m_entry_names;
    std::unordered_map<std::string, OpenGdtFile> m_files;
};
