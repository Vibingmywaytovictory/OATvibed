#include "Game/IW3/Sound/SoundAliasDumperIW3.h"

#include "Csv/CsvStream.h"
#include "Game/IW3/SoundConstantsIW3.h"
#include "SearchPath/MockOutputPath.h"
#include "SearchPath/MockSearchPath.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace IW3;

namespace
{
    TEST_CASE("SoundAliasDumperIW3: Dumps SDK-compatible sound alias rows", "[iw3][sound-alias][assetdumper]")
    {
        LoadedSound loadedSound{};
        loadedSound.name = "weapons/test_loaded.wav";

        SoundFile soundFiles[2]{};
        soundFiles[0].type = SAT_LOADED;
        soundFiles[0].exists = true;
        soundFiles[0].u.loadSnd = &loadedSound;
        soundFiles[1].type = SAT_STREAMED;
        soundFiles[1].exists = true;
        soundFiles[1].u.streamSnd.dir = "music";
        soundFiles[1].u.streamSnd.name = "test_streamed.mp3";

        SndCurve customCurve{};
        customCurve.filename = "weapon1";

        SpeakerMap defaultSpeakerMap{};
        defaultSpeakerMap.isDefault = true;
        defaultSpeakerMap.name = "default";

        SpeakerMap customSpeakerMap{};
        customSpeakerMap.name = "music";
        for (auto sourceMode = 0; sourceMode < 2; sourceMode++)
        {
            for (auto outputMode = 0; outputMode < 2; outputMode++)
            {
                auto& channelMap = customSpeakerMap.channelMaps[sourceMode][outputMode];
                channelMap.speakerCount = outputMode == 0 ? 2 : 6;
                for (auto outputChannel = 0; outputChannel < channelMap.speakerCount; outputChannel++)
                {
                    auto& speaker = channelMap.speakers[outputChannel];
                    speaker.speaker = outputChannel;
                    speaker.numLevels = sourceMode + 1;
                    for (auto inputChannel = 0; inputChannel <= sourceMode; inputChannel++)
                        speaker.levels[inputChannel] = 0.1f * static_cast<float>(1 + sourceMode + outputMode + outputChannel + inputChannel);
                }
            }
        }

        snd_alias_t aliases[2]{};
        aliases[0].aliasName = "test_alias";
        aliases[0].subtitle = "A subtitle, with a comma";
        aliases[0].secondaryAliasName = "secondary_alias";
        aliases[0].chainAliasName = "chain_alias";
        aliases[0].soundFile = &soundFiles[0];
        aliases[0].volMin = 0.25f;
        aliases[0].volMax = 0.75f;
        aliases[0].pitchMin = 0.9f;
        aliases[0].pitchMax = 1.1f;
        aliases[0].distMin = 64.0f;
        aliases[0].distMax = 512.0f;
        aliases[0].flags = (18 << SND_ALIAS_FLAG_CHANNEL_SHIFT) | (SAT_LOADED << SND_ALIAS_FLAG_TYPE_SHIFT) | SND_ALIAS_FLAG_LOOPING | SND_ALIAS_FLAG_MASTER
                           | SND_ALIAS_FLAG_FULL_DRY_LEVEL | SND_ALIAS_FLAG_NO_WET_LEVEL;
        aliases[0].slavePercentage = 1.0f;
        aliases[0].probability = 0.8f;
        aliases[0].lfePercentage = 0.2f;
        aliases[0].centerPercentage = 0.3f;
        aliases[0].startDelay = 25;
        aliases[0].volumeFalloffCurve = &customCurve;
        aliases[0].envelopMin = 0.1f;
        aliases[0].envelopMax = 0.9f;
        aliases[0].envelopPercentage = 0.4f;
        aliases[0].speakerMap = &defaultSpeakerMap;

        aliases[1].aliasName = "test_alias";
        aliases[1].soundFile = &soundFiles[1];
        aliases[1].volMin = 1.0f;
        aliases[1].volMax = 1.0f;
        aliases[1].pitchMin = 1.0f;
        aliases[1].pitchMax = 1.0f;
        aliases[1].distMin = 120.0f;
        aliases[1].distMax = 600.0f;
        aliases[1].flags = (10 << SND_ALIAS_FLAG_CHANNEL_SHIFT) | (SAT_STREAMED << SND_ALIAS_FLAG_TYPE_SHIFT) | SND_ALIAS_FLAG_LOOPING | SND_ALIAS_FLAG_SLAVE
                           | SND_ALIAS_FLAG_RANDOM_LOOPING;
        aliases[1].slavePercentage = 0.5f;
        aliases[1].probability = 1.0f;
        aliases[1].speakerMap = &customSpeakerMap;

        snd_alias_list_t aliasList{};
        aliasList.aliasName = "test_alias";
        aliasList.head = aliases;
        aliasList.count = 2;

        Zone zone("DumpingZone", 0, GameId::IW3, GamePlatform::PC);
        zone.m_pools.AddAsset(std::make_unique<XAssetInfo<snd_alias_list_t>>(ASSET_TYPE_SOUND, aliasList.aliasName, &aliasList));

        MockSearchPath mockObjPath;
        MockOutputPath mockOutput;
        AssetDumpingContext context(zone, "", mockOutput, mockObjPath, std::nullopt);

        sound_alias::DumperIW3 dumper;
        dumper.Dump(context);

        const auto* file = mockOutput.GetMockedFile("soundaliases/DumpingZone.csv");
        REQUIRE(file != nullptr);

        std::istringstream input(file->AsString());
        CsvInputStream csv(input);
        std::vector<std::string> row;

        REQUIRE(csv.NextRow(row));
        REQUIRE(row.size() == 30);
        REQUIRE(row[0] == "name");
        REQUIRE(row[29] == "chainaliasname");

        REQUIRE(csv.NextRow(row));
        REQUIRE(row.size() == 30);
        REQUIRE(row[0] == "test_alias");
        REQUIRE(row[1] == "1");
        REQUIRE(row[2] == "weapons/test_loaded.wav");
        REQUIRE(row[3] == "0.25");
        REQUIRE(row[4] == "0.75");
        REQUIRE(row[5].empty());
        REQUIRE(row[10] == "weapon");
        REQUIRE(row[11] == "loaded");
        REQUIRE(row[13] == "looping");
        REQUIRE(row[14] == "master");
        REQUIRE(row[16] == "A subtitle, with a comma");
        REQUIRE(row[18] == "secondary_alias");
        REQUIRE(row[19] == "weapon1");
        REQUIRE(row[20] == "25");
        REQUIRE(row[21].empty());
        REQUIRE(row[22] == "fulldrylevel nowetlevel");
        REQUIRE(row[29] == "chain_alias");

        REQUIRE(csv.NextRow(row));
        REQUIRE(row.size() == 30);
        REQUIRE(row[0] == "test_alias");
        REQUIRE(row[1] == "2");
        REQUIRE(row[2] == "music/test_streamed.mp3");
        REQUIRE(row[10] == "menu");
        REQUIRE(row[11] == "streamed");
        REQUIRE(row[13] == "rlooping");
        REQUIRE(row[14] == "0.5");
        REQUIRE(row[21] == "music");
        REQUIRE_FALSE(csv.NextRow(row));

        const auto* speakerMapFile = mockOutput.GetMockedFile("soundaliases/music.spkrmap");
        REQUIRE(speakerMapFile != nullptr);
        REQUIRE(speakerMapFile->AsString().starts_with("SPKRMAP\n\nMONOSOURCE LEFTSPEAKER 0.1000\n"));
        REQUIRE(speakerMapFile->AsString().find("RIGHTSOURCE RIGHTSURROUNDSPEAKER 0.9000\n") != std::string::npos);
        REQUIRE(mockOutput.GetMockedFile("soundaliases/default.spkrmap") == nullptr);
    }

    TEST_CASE("SoundAliasDumperIW3: Writes values the native csv parser can read back", "[iw3][sound-alias][assetdumper]")
    {
        LoadedSound loadedSound{};
        loadedSound.name = "weapons/lone_alias.wav";

        SoundFile soundFile{};
        soundFile.type = SAT_LOADED;
        soundFile.exists = true;
        soundFile.u.loadSnd = &loadedSound;

        snd_alias_t alias{};
        alias.aliasName = "lone_alias";
        alias.soundFile = &soundFile;
        alias.volMin = 1.0f;
        alias.volMax = 1.0f;
        // Small enough that the default float formatting would reach for scientific notation
        alias.pitchMin = 0.0000021f;
        alias.pitchMax = 1.0f;
        alias.flags = (18 << SND_ALIAS_FLAG_CHANNEL_SHIFT) | (SAT_LOADED << SND_ALIAS_FLAG_TYPE_SHIFT);

        snd_alias_list_t aliasList{};
        aliasList.aliasName = "lone_alias";
        aliasList.head = &alias;
        aliasList.count = 1;

        Zone zone("DumpingZone", 0, GameId::IW3, GamePlatform::PC);
        zone.m_pools.AddAsset(std::make_unique<XAssetInfo<snd_alias_list_t>>(ASSET_TYPE_SOUND, aliasList.aliasName, &aliasList));

        MockSearchPath mockObjPath;
        MockOutputPath mockOutput;
        AssetDumpingContext context(zone, "", mockOutput, mockObjPath, std::nullopt);

        sound_alias::DumperIW3 dumper;
        dumper.Dump(context);

        const auto* file = mockOutput.GetMockedFile("soundaliases/DumpingZone.csv");
        REQUIRE(file != nullptr);

        std::istringstream input(file->AsString());
        CsvInputStream csv(input);
        std::vector<std::string> row;

        REQUIRE(csv.NextRow(row));
        REQUIRE(csv.NextRow(row));
        REQUIRE(row.size() == 30);

        // An alias with no variants leaves the sequence empty, the way the stock files do
        REQUIRE(row[1].empty());

        // The native parser stops at the 'e' of an exponent, so the value has to be written out in full
        REQUIRE(row[6].find('e') == std::string::npos);
        REQUIRE(row[6] == "0.0000021");

        // Columns the stock files leave empty rather than writing a zero into
        REQUIRE(row[12].empty()); // probability
        REQUIRE(row[20].empty()); // startdelay
        REQUIRE(row[23].empty()); // lfe percentage
        REQUIRE(row[24].empty()); // center percentage
        REQUIRE(row[26].empty()); // envelop_min
        REQUIRE(row[27].empty()); // envelop_max
        REQUIRE(row[28].empty()); // envelop percentage
    }
} // namespace
