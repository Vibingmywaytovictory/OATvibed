#include "AutoSearchPaths.h"

#include "IW3/AutoSearchPathsIW3.h"
#include "IW4/AutoSearchPathsIW4.h"
#include "IW4/InfoString/InfoStringToStructConverter.h"
#include "IW5/AutoSearchPathsIW5.h"
#include "QOS/AutoSearchPathsQOS.h"
#include "T4/AutoSearchPathsT4.h"
#include "T5/AutoSearchPathsT5.h"
#include "T6/AutoSearchPathsT6.h"
#include "Utils/StringUtils.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <optional>
#include <utility>

namespace fs = std::filesystem;

namespace
{
    std::optional<std::string> FindGameRootFolder(const std::string& zoneParentPath, const std::vector<std::string>& zoneDirs)
    {
        std::string lowerZoneParentPath(zoneParentPath);
        utils::MakeStringLowerCase(lowerZoneParentPath);

        for (const auto& dir : zoneDirs)
        {
            std::string normalizedDir(dir);
            utils::MakeStringLowerCase(normalizedDir);

            if (lowerZoneParentPath.ends_with(normalizedDir) && lowerZoneParentPath[lowerZoneParentPath.size() - dir.size() - 1] == '/')
                return zoneParentPath.substr(0, zoneParentPath.size() - dir.size() - 1);
        }

        return std::nullopt;
    }

    // A mod fastfile lives in <game>/mods/<mod>. Its own folder holds the mod iwds, but images and other data the mod
    // did not replace still come from the base game, so the game root has to be found from here as well.
    std::optional<std::string> FindGameRootFolderOfMod(const std::string& zoneParentPath)
    {
        const auto modNameSeparator = zoneParentPath.find_last_of('/');
        if (modNameSeparator == std::string::npos || modNameSeparator == 0)
            return std::nullopt;

        auto modsFolder = zoneParentPath.substr(0, modNameSeparator);
        const auto modsFolderSeparator = modsFolder.find_last_of('/');
        if (modsFolderSeparator == std::string::npos)
            return std::nullopt;

        auto modsFolderName = modsFolder.substr(modsFolderSeparator + 1);
        utils::MakeStringLowerCase(modsFolderName);
        if (modsFolderName != "mods")
            return std::nullopt;

        return modsFolder.substr(0, modsFolderSeparator);
    }
} // namespace

std::vector<std::string> AutoSearchPaths::GetSearchPathsForZonePath(const std::string& zonePath) const
{
    auto folderName = fs::absolute(fs::path(zonePath)).parent_path().string();
    std::ranges::replace(folderName, '\\', '/');

    std::vector<std::string> result;

    auto maybeGameRootFolder = FindGameRootFolder(folderName, RecognizedZoneDirs());
    if (!maybeGameRootFolder)
    {
        maybeGameRootFolder = FindGameRootFolderOfMod(folderName);

        // The mod folder itself comes first so that anything the mod ships takes precedence over the base game
        if (maybeGameRootFolder)
            result.emplace_back(folderName);
    }

    if (!maybeGameRootFolder)
        return {folderName};

    con::debug("Detected game directory: {}", *maybeGameRootFolder);

    const fs::path gameRootFolderPath(*maybeGameRootFolder);

    for (const auto& dir : RecognizedZoneDirs())
    {
        auto dirPath = fs::weakly_canonical(gameRootFolderPath / dir);

        if (fs::is_directory(dirPath))
            result.emplace_back(dirPath.string());
    }

    for (const auto& dir : AdditionalSearchPaths())
    {
        auto dirPath = fs::weakly_canonical(gameRootFolderPath / dir);

        if (fs::is_directory(dirPath))
            result.emplace_back(dirPath.string());
    }

    return result;
}

AutoSearchPaths* AutoSearchPaths::GetForGame(GameId gameId)
{
    static AutoSearchPaths* autoSearchPaths[]{
        new AutoSearchPathsIW3(),
        new AutoSearchPathsIW4(),
        new AutoSearchPathsIW5(),
        new AutoSearchPathsQOS(),
        new AutoSearchPathsT4(),
        new AutoSearchPathsT5(),
        new AutoSearchPathsT6(),
    };
    static_assert(std::extent_v<decltype(autoSearchPaths)> == static_cast<unsigned>(GameId::COUNT));
    assert(static_cast<unsigned>(gameId) < static_cast<unsigned>(GameId::COUNT));

    return autoSearchPaths[std::to_underlying(gameId)];
}
