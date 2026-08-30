#include "GdtOutputStreamCollection.h"

#include "Utils/Logging/Log.h"

#include <format>
#include <utility>

bool GdtEntryNames::Add(const std::string& entryName)
{
    return m_written.emplace(entryName).second;
}

GdtOutputStreamCollection::GdtOutputStreamCollection(IOutputPath& outputPath, std::string zoneName, GdtEntryNames& entryNames)
    : m_output_path(outputPath),
      m_zone_name(std::move(zoneName)),
      m_entry_names(entryNames)
{
    // The zone definition references the ungrouped file by name, so it has to exist even when no dumper writes to it.
    // Only the group files are created on demand.
    GetStreamForGroup(std::string());
}

GdtOutputStreamCollection::~GdtOutputStreamCollection()
{
    for (auto& [group, openFile] : m_files)
    {
        if (openFile.m_gdt)
            openFile.m_gdt->EndStream();
    }
}

void GdtOutputStreamCollection::WriteEntry(const GdtEntry& entry)
{
    WriteEntry(std::string(), entry);
}

void GdtOutputStreamCollection::WriteEntry(const std::string& group, const GdtEntry& entry)
{
    if (!m_entry_names.Add(entry.m_name))
    {
        con::warn("Skipping gdt entry \"{}\" of zone \"{}\": an entry of that name was written for an earlier zone", entry.m_name, m_zone_name);
        return;
    }

    auto* stream = GetStreamForGroup(group);
    if (stream)
        stream->WriteEntry(entry);
}

GdtOutputStream* GdtOutputStreamCollection::GetStreamForGroup(const std::string& group)
{
    const auto existingFile = m_files.find(group);
    if (existingFile != m_files.end())
        return existingFile->second.m_gdt.get();

    const auto fileName = group.empty() ? std::format("source_data/{}.gdt", m_zone_name) : std::format("source_data/{}_{}.gdt", m_zone_name, group);

    auto file = m_output_path.Open(fileName);
    if (!file)
    {
        con::error("Failed to open gdt file \"{}\"", fileName);

        // Remember the failure so the next entry of the group does not report it again
        m_files.emplace(group, OpenGdtFile());
        return nullptr;
    }

    auto gdt = std::make_unique<GdtOutputStream>(*file);
    gdt->BeginStream();

    // No version entry: AssetManager resolves every entry against a gdf of the same name and there is no version.gdf,
    // so it would reject the file. Reading a gdt without one is fine, the version is optional to GdtReader and nothing
    // consumes it.

    auto* gdtPtr = gdt.get();
    m_files.emplace(group,
                    OpenGdtFile{
                        .m_file = std::move(file),
                        .m_gdt = std::move(gdt),
                    });

    return gdtPtr;
}
