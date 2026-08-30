#include "LocalizeDumperIW3.h"

#include "Dumping/Localize/StringFileDumper.h"
#include "Localize/LocalizeCommon.h"
#include "Utils/Logging/Log.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <map>
#include <string>
#include <utility>
#include <vector>

using namespace IW3;

namespace
{
    /**
     * \brief Splits a localize asset name back into the string file it came from and the reference inside it.
     *
     * The game names every string FILENAME_REFERENCE, so MPUI_BOG was authored as BOG in mpui.str. Dumping all of
     * them into one <zone>.str and compiling that would rename the lot to MOD_* and every @MPUI_... lookup in the
     * menus would miss, which is why the names have to be taken apart again.
     *
     * \return False for a name without a prefix, which cannot be attributed to a file.
     */
    bool SplitLocalizeName(const std::string& name, std::string& fileName, std::string& reference)
    {
        const auto separator = name.find('_');
        if (separator == std::string::npos || separator == 0 || separator + 1 >= name.size())
            return false;

        fileName = name.substr(0, separator);
        std::ranges::transform(fileName,
                               fileName.begin(),
                               [](const char c)
                               {
                                   return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                               });
        reference = name.substr(separator + 1);

        return true;
    }
} // namespace

namespace localize
{
    void DumperIW3::Dump(AssetDumpingContext& context)
    {
        auto localizeAssets = context.m_zone.m_pools.PoolAssets<AssetLocalize>();
        if (localizeAssets.empty())
            return;

        // Sorted so the dump does not depend on the order the pool happens to hold
        std::map<std::string, std::vector<std::pair<std::string, std::string>>> entriesByFile;
        for (const auto* localizeEntry : localizeAssets)
        {
            std::string fileName;
            std::string reference;
            if (SplitLocalizeName(localizeEntry->m_name, fileName, reference))
            {
                entriesByFile[fileName].emplace_back(std::move(reference), localizeEntry->Asset()->value);
            }
            else
            {
                // Nothing better to do with it than keep it whole in a file named after the zone
                con::warn("Localized string \"{}\" has no file name prefix, dumping it to \"{}.str\"", localizeEntry->m_name, context.m_zone.m_name);
                entriesByFile[context.m_zone.m_name].emplace_back(localizeEntry->m_name, localizeEntry->Asset()->value);
            }
        }

        const auto language = LocalizeCommon::GetNameOfLanguage(context.m_zone.m_language);

        for (const auto& [fileName, entries] : entriesByFile)
        {
            const auto assetFile = context.OpenAssetFile(std::format("{}/localizedstrings/{}.str", language, fileName));
            if (!assetFile)
            {
                con::error("Could not create string file \"{}.str\" for dumping localized strings of zone '{}'", fileName, context.m_zone.m_name);
                continue;
            }

            StringFileDumper stringFileDumper(context.m_zone, *assetFile);

            stringFileDumper.SetLanguageName(language);

            // Magic string. Original string files do have this config file. The purpose of the config file is unknown though.
            stringFileDumper.SetConfigFile(R"(C:/trees/cod3/cod3/bin/StringEd.cfg)");

            stringFileDumper.SetNotes("");

            for (const auto& [reference, value] : entries)
                stringFileDumper.WriteLocalizeEntry(reference, value);

            stringFileDumper.Finalize();
        }

        context.IncrementProgress();
    }
} // namespace localize
