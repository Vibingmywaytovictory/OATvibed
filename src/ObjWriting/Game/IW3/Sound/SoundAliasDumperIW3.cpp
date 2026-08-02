#include "SoundAliasDumperIW3.h"

#include "Utils/Logging/Log.h"

#include <array>
#include <format>
#include <ostream>
#include <string>

using namespace IW3;

namespace
{
    // snd_alias_t::flags bit layout of the IW3 engine
    constexpr auto FLAG_LOOPING = 1 << 0;
    constexpr auto FLAG_MASTER = 1 << 1;
    constexpr auto FLAG_SLAVE = 1 << 2;
    constexpr auto FLAG_FULL_DRY_LEVEL = 1 << 3;
    constexpr auto FLAG_NO_WET_LEVEL = 1 << 4;
    constexpr auto CHANNEL_SHIFT = 8;
    constexpr auto CHANNEL_MASK = 0x3F;

    // In the order the engine registers them, which is the order of the stock soundaliases/channels.def
    constexpr std::array CHANNEL_NAMES{
        "physics", "auto",   "auto2",   "autodog",  "bulletimpact", "bulletwhizby", "element",  "auto2d",       "vehicle",  "vehiclelimited", "menu",
        "body",    "body2d", "reload",  "reload2d", "item",         "effects1",     "effects2", "weapon",       "weapon2d", "nonshock",       "voice",
        "local",   "local2", "ambient", "hurt",     "player1",      "player2",      "music",    "musicnopause", "mission",  "announcer",      "shellshock",
    };

    // The header of the stock soundaliases csv files. The linker maps values by these column names.
    constexpr auto CSV_HEADER = "name,sequence,file,vol_min,vol_max,vol_mod,pitch_min,pitch_max,dist_min,dist_max,channel,type,probability,loop,"
                                "masterslave,loadspec,subtitle,compression,secondaryaliasname,volumefalloffcurve,startdelay,speakermap,reverb,"
                                "lfe percentage,center percentage,platform,envelop_min,envelop_max,envelop percentage,conversion";

    std::string FloatValue(const float value)
    {
        auto result = std::format("{}", value);

        // The native csv parser does not read scientific notation
        if (result.find('e') != std::string::npos)
        {
            result = std::format("{:.8f}", value);
            const auto lastNonZero = result.find_last_not_of('0');
            result.erase(result[lastNonZero] == '.' ? lastNonZero : lastNonZero + 1);
        }

        return result;
    }

    std::string CsvValue(const std::string& value)
    {
        if (value.find_first_of(",\"") == std::string::npos)
            return value;

        std::string escaped = "\"";
        for (const auto c : value)
        {
            if (c == '"')
                escaped += '"';
            escaped += c;
        }
        escaped += '"';
        return escaped;
    }

    const char* ChannelName(const int flags)
    {
        const auto channelIndex = static_cast<size_t>((flags >> CHANNEL_SHIFT) & CHANNEL_MASK);
        if (channelIndex < CHANNEL_NAMES.size())
            return CHANNEL_NAMES[channelIndex];

        return "";
    }

    std::string SoundFilePath(const SoundFile* soundFile)
    {
        if (!soundFile)
            return "";

        if (soundFile->type == SAT_STREAMED)
        {
            const auto& streamed = soundFile->u.streamSnd;
            const std::string dir = streamed.dir ? streamed.dir : "";
            const std::string name = streamed.name ? streamed.name : "";
            return dir.empty() ? name : dir + "/" + name;
        }

        if (soundFile->type == SAT_LOADED && soundFile->u.loadSnd && soundFile->u.loadSnd->name)
            return soundFile->u.loadSnd->name;

        return "";
    }

    void WriteAliasRow(std::ostream& stream, const snd_alias_t& alias, const int sequence)
    {
        const auto masterslave = (alias.flags & FLAG_MASTER)  ? std::string("master")
                                 : (alias.flags & FLAG_SLAVE) ? FloatValue(alias.slavePercentage)
                                                              : std::string();

        std::string reverb;
        if (alias.flags & FLAG_NO_WET_LEVEL)
            reverb = "nowetlevel";
        if (alias.flags & FLAG_FULL_DRY_LEVEL)
            reverb += reverb.empty() ? "fulldrylevel" : " fulldrylevel";

        stream << CsvValue(alias.aliasName ? alias.aliasName : "");                                          // name
        stream << ',' << (sequence != 0 ? std::to_string(sequence) : "");                                    // sequence
        stream << ',' << CsvValue(SoundFilePath(alias.soundFile));                                           // file
        stream << ',' << FloatValue(alias.volMin);                                                           // vol_min
        stream << ',' << FloatValue(alias.volMax);                                                           // vol_max
        stream << ',';                                                                                       // vol_mod (baked in at compile time)
        stream << ',' << FloatValue(alias.pitchMin);                                                         // pitch_min
        stream << ',' << FloatValue(alias.pitchMax);                                                         // pitch_max
        stream << ',' << FloatValue(alias.distMin);                                                          // dist_min
        stream << ',' << FloatValue(alias.distMax);                                                          // dist_max
        stream << ',' << ChannelName(alias.flags);                                                           // channel
        stream << ',' << (alias.soundFile && alias.soundFile->type == SAT_STREAMED ? "streamed" : "loaded"); // type
        stream << ',' << (alias.probability != 0.0f ? FloatValue(alias.probability) : "");                   // probability
        stream << ',' << ((alias.flags & FLAG_LOOPING) ? "looping" : "nonlooping");                          // loop
        stream << ',' << masterslave;                                                                        // masterslave
        stream << ',';                                                                                       // loadspec (compile time filter)
        stream << ',' << CsvValue(alias.subtitle ? alias.subtitle : "");                                     // subtitle
        stream << ',';                                                                                       // compression (not stored)
        stream << ',' << CsvValue(alias.secondaryAliasName ? alias.secondaryAliasName : "");                 // secondaryaliasname
        stream << ',' << (alias.volumeFalloffCurve && alias.volumeFalloffCurve->filename ? alias.volumeFalloffCurve->filename : ""); // volumefalloffcurve
        stream << ',' << (alias.startDelay != 0 ? std::to_string(alias.startDelay) : "");                                            // startdelay
        stream << ',' << (alias.speakerMap && !alias.speakerMap->isDefault && alias.speakerMap->name ? alias.speakerMap->name : ""); // speakermap
        stream << ',' << reverb;                                                                                                     // reverb
        stream << ',' << (alias.lfePercentage != 0.0f ? FloatValue(alias.lfePercentage) : "");                                       // lfe percentage
        stream << ',' << (alias.centerPercentage != 0.0f ? FloatValue(alias.centerPercentage) : "");                                 // center percentage
        stream << ',';                                                                                                               // platform
        stream << ',' << (alias.envelopMin != 0.0f ? FloatValue(alias.envelopMin) : "");                                             // envelop_min
        stream << ',' << (alias.envelopMax != 0.0f ? FloatValue(alias.envelopMax) : "");                                             // envelop_max
        stream << ',' << (alias.envelopPercentage != 0.0f ? FloatValue(alias.envelopPercentage) : "");                               // envelop percentage
        stream << ',';                                                                                                               // conversion
        stream << '\n';
    }
} // namespace

namespace sound
{
    void AliasDumperIW3::Dump(AssetDumpingContext& context)
    {
        const auto soundAssets = context.m_zone.m_pools.PoolAssets<AssetSound>();
        if (soundAssets.empty())
            return;

        const auto assetFile = context.OpenAssetFile(std::format("soundaliases/{}.csv", context.m_zone.m_name));
        if (!assetFile)
        {
            con::error("Could not create soundaliases csv for zone '{}'", context.m_zone.m_name);
            context.IncrementProgress();
            return;
        }

        auto& stream = *assetFile;
        stream << "# Dumped from fastfile \"" << context.m_zone.m_name << "\".\n";
        stream << CSV_HEADER << "\n";

        auto aliasCount = 0u;
        for (const auto* assetInfo : soundAssets)
        {
            if (assetInfo->IsReference())
                continue;

            const auto* aliasList = assetInfo->Asset();
            if (!aliasList->head)
                continue;

            for (auto aliasIndex = 0; aliasIndex < aliasList->count; aliasIndex++)
            {
                // The compiled asset does not retain the csv sequence value, but the linker refuses same-named
                // rows without distinct sequences, so number the variants 1..n the way the stock files do.
                const auto sequence = aliasList->head[aliasIndex].sequence != 0 ? aliasList->head[aliasIndex].sequence
                                      : aliasList->count > 1                    ? aliasIndex + 1
                                                                                : 0;
                WriteAliasRow(stream, aliasList->head[aliasIndex], sequence);
                aliasCount++;
            }
        }

        con::info("Dumped {} sound aliases to \"soundaliases/{}.csv\"", aliasCount, context.m_zone.m_name);
        context.IncrementProgress();
    }
} // namespace sound
