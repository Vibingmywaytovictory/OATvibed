#include "Game/IW3/Sound/SoundAliasDumperIW3.h"

#include "SearchPath/MockOutputPath.h"
#include "SearchPath/MockSearchPath.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

using namespace IW3;

namespace
{
    TEST_CASE("SoundAliasDumperIW3: Dumps a soundaliases csv the native linker accepts", "[iw3][sound][assetdumper]")
    {
        LoadedSound loadedSound{};
        loadedSound.name = "weapons/ak47_fire.wav";

        SoundFile loadedFile{};
        loadedFile.type = SAT_LOADED;
        loadedFile.exists = 1;
        loadedFile.u.loadSnd = &loadedSound;

        SoundFile streamedFile{};
        streamedFile.type = SAT_STREAMED;
        streamedFile.exists = 1;
        streamedFile.u.streamSnd = {"music", "mission_theme.mp3"};

        SndCurve curve{};
        curve.filename = "weapon2";

        // channel 18 (weapon), loaded type, slave, nowetlevel
        snd_alias_t fireVariant1{};
        fireVariant1.aliasName = "weap_ak47_fire";
        fireVariant1.soundFile = &loadedFile;
        fireVariant1.volMin = 0.85f;
        fireVariant1.volMax = 0.9f;
        fireVariant1.pitchMin = 0.95f;
        fireVariant1.pitchMax = 1.05f;
        fireVariant1.distMin = 10.0f;
        fireVariant1.distMax = 1500.0f;
        fireVariant1.flags = (18 << 8) | (SAT_LOADED << 6) | (1 << 4) | (1 << 2);
        fireVariant1.slavePercentage = 0.85f;
        fireVariant1.probability = 0.5f;
        fireVariant1.volumeFalloffCurve = &curve;

        auto fireVariant2 = fireVariant1;
        fireVariant2.probability = 0.0f;

        snd_alias_t variants[]{fireVariant1, fireVariant2};
        snd_alias_list_t fireList{"weap_ak47_fire", variants, 2};

        // channel 28 (music), streamed, looping, master, fulldrylevel
        snd_alias_t musicAlias{};
        musicAlias.aliasName = "mission_music";
        musicAlias.subtitle = "subtitle, with comma";
        musicAlias.soundFile = &streamedFile;
        musicAlias.volMin = 1.0f;
        musicAlias.volMax = 1.0f;
        musicAlias.pitchMin = 1.0f;
        musicAlias.pitchMax = 1.0f;
        musicAlias.distMin = 120.0f;
        musicAlias.distMax = 1250.0f;
        musicAlias.flags = (28 << 8) | (SAT_STREAMED << 6) | (1 << 3) | (1 << 1) | (1 << 0);
        musicAlias.startDelay = 500;

        snd_alias_list_t musicList{"mission_music", &musicAlias, 1};

        Zone zone("MockZone", 0, GameId::IW3, GamePlatform::PC);
        zone.m_pools.AddAsset(std::make_unique<XAssetInfo<snd_alias_list_t>>(ASSET_TYPE_SOUND, fireList.aliasName, &fireList));
        zone.m_pools.AddAsset(std::make_unique<XAssetInfo<snd_alias_list_t>>(ASSET_TYPE_SOUND, musicList.aliasName, &musicList));

        MockSearchPath mockObjPath;
        MockOutputPath mockOutput;
        AssetDumpingContext context(zone, "", mockOutput, mockObjPath, std::nullopt);

        sound::AliasDumperIW3 dumper;
        dumper.Dump(context);

        const auto* file = mockOutput.GetMockedFile("soundaliases/MockZone.csv");
        REQUIRE(file);

        constexpr auto expectedOutput =
            R"(# Dumped from fastfile "MockZone".
name,sequence,file,vol_min,vol_max,vol_mod,pitch_min,pitch_max,dist_min,dist_max,channel,type,probability,loop,masterslave,loadspec,subtitle,compression,secondaryaliasname,volumefalloffcurve,startdelay,speakermap,reverb,lfe percentage,center percentage,platform,envelop_min,envelop_max,envelop percentage,conversion
weap_ak47_fire,1,weapons/ak47_fire.wav,0.85,0.9,,0.95,1.05,10,1500,weapon,loaded,0.5,nonlooping,0.85,,,,,weapon2,,,nowetlevel,,,,,,,
weap_ak47_fire,2,weapons/ak47_fire.wav,0.85,0.9,,0.95,1.05,10,1500,weapon,loaded,,nonlooping,0.85,,,,,weapon2,,,nowetlevel,,,,,,,
mission_music,,music/mission_theme.mp3,1,1,,1,1,120,1250,music,streamed,,looping,master,,"subtitle, with comma",,,,500,,fulldrylevel,,,,,,,
)";
        REQUIRE(file->AsString() == expectedOutput);
    }
} // namespace
