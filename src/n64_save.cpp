#include "pch.h"
#include "n64_save.h"

#include "high_score.h"
#include "options.h"

namespace
{
	constexpr uint32_t OPTIONS_MAGIC = 0x4F50544E; // OPTN
	constexpr uint32_t HIGHSCORE_MAGIC = 0x48495343; // HISC
	constexpr uint16_t SAVE_VERSION = 1;

	struct N64OptionsBlob
	{
		uint32_t Magic;
		uint16_t Version;
		uint16_t Reserved;
		uint8_t Sounds;
		uint8_t Music;
		uint8_t Players;
		uint8_t UniformScaling;
		uint8_t LinearFiltering;
		uint8_t ShowMenu;
		uint8_t UncappedUpdatesPerSecond;
		uint8_t ReservedFlags;
		int16_t FramesPerSecond;
		int16_t UpdatesPerSecond;
		int16_t SoundChannels;
		uint16_t KeyLeftFlipper;
		uint16_t KeyRightFlipper;
		uint16_t KeyPlunger;
		uint16_t KeyLeftTableBump;
		uint16_t KeyRightTableBump;
		uint16_t KeyBottomTableBump;
	};

	struct N64HighScoreEntry
	{
		char Name[32];
		int32_t Score;
	};

	struct N64HighScoreBlob
	{
		uint32_t Magic;
		uint16_t Version;
		uint16_t Count;
		N64HighScoreEntry Scores[5];
	};

	constexpr eepfs_entry_t SaveEntries[] =
	{
		{"/pinball/options.bin", sizeof(N64OptionsBlob), true, false},
		{"/pinball/highscores.bin", sizeof(N64HighScoreBlob), true, false},
	};

	bool SaveInitialized = false;
	bool SaveAvailable = false;
}

void n64_save::Initialize()
{
	if (SaveInitialized)
		return;
	SaveInitialized = true;

	auto eepromType = eeprom_present();
	if (eepromType == EEPROM_NONE)
	{
		debugf("n64_save: EEPROM not detected, persistence disabled.\n");
		return;
	}

	int rc = eepfs_init(SaveEntries, sizeof(SaveEntries) / sizeof(SaveEntries[0]));
	if (rc != EEPFS_ESUCCESS)
	{
		debugf("n64_save: eepfs_init failed (%d), persistence disabled.\n", rc);
		return;
	}

	if (!eepfs_verify_signature())
	{
		debugf("n64_save: invalid EEPROM signature, wiping.\n");
		eepfs_wipe();
		PumpWrites();
	}

	SaveAvailable = true;
}

void n64_save::Shutdown()
{
	if (!SaveInitialized)
		return;

	PumpWrites();
	if (SaveAvailable)
		eepfs_close();
	SaveAvailable = false;
	SaveInitialized = false;
}

bool n64_save::IsAvailable()
{
	return SaveAvailable;
}

void n64_save::PumpWrites()
{
	while (eeprom_is_busy())
	{
		mixer_try_play();
	}
}

bool n64_save::LoadOptions(optionsStruct& options)
{
	if (!SaveAvailable)
		return false;

	N64OptionsBlob blob{};
	auto rc = eepfs_read("/pinball/options.bin", &blob, sizeof(blob));
	if (rc != EEPFS_ESUCCESS || blob.Magic != OPTIONS_MAGIC || blob.Version != SAVE_VERSION)
		return false;

	options.Sounds = blob.Sounds != 0;
	options.Music = blob.Music != 0;
	options.Players = blob.Players;
	options.UniformScaling = blob.UniformScaling != 0;
	options.LinearFiltering = blob.LinearFiltering != 0;
	options.ShowMenu = blob.ShowMenu != 0;
	options.UncappedUpdatesPerSecond = blob.UncappedUpdatesPerSecond != 0;
	options.FramesPerSecond = blob.FramesPerSecond;
	options.UpdatesPerSecond = blob.UpdatesPerSecond;
	options.SoundChannels = blob.SoundChannels;

	options.Key.LeftFlipper = blob.KeyLeftFlipper;
	options.Key.RightFlipper = blob.KeyRightFlipper;
	options.Key.Plunger = blob.KeyPlunger;
	options.Key.LeftTableBump = blob.KeyLeftTableBump;
	options.Key.RightTableBump = blob.KeyRightTableBump;
	options.Key.BottomTableBump = blob.KeyBottomTableBump;
	return true;
}

void n64_save::SaveOptions(const optionsStruct& options)
{
	if (!SaveAvailable)
		return;

	N64OptionsBlob blob{};
	blob.Magic = OPTIONS_MAGIC;
	blob.Version = SAVE_VERSION;
	blob.Sounds = options.Sounds ? 1 : 0;
	blob.Music = options.Music ? 1 : 0;
	blob.Players = static_cast<uint8_t>(options.Players);
	blob.UniformScaling = options.UniformScaling ? 1 : 0;
	blob.LinearFiltering = options.LinearFiltering ? 1 : 0;
	blob.ShowMenu = options.ShowMenu ? 1 : 0;
	blob.UncappedUpdatesPerSecond = options.UncappedUpdatesPerSecond ? 1 : 0;
	blob.FramesPerSecond = static_cast<int16_t>(options.FramesPerSecond);
	blob.UpdatesPerSecond = static_cast<int16_t>(options.UpdatesPerSecond);
	blob.SoundChannels = static_cast<int16_t>(options.SoundChannels);
	blob.KeyLeftFlipper = options.Key.LeftFlipper;
	blob.KeyRightFlipper = options.Key.RightFlipper;
	blob.KeyPlunger = options.Key.Plunger;
	blob.KeyLeftTableBump = options.Key.LeftTableBump;
	blob.KeyRightTableBump = options.Key.RightTableBump;
	blob.KeyBottomTableBump = options.Key.BottomTableBump;

	eepfs_write("/pinball/options.bin", &blob, sizeof(blob));
}

bool n64_save::LoadHighScores(high_score_struct* table, int count)
{
	if (!SaveAvailable || !table || count <= 0 || count > 5)
		return false;

	N64HighScoreBlob blob{};
	auto rc = eepfs_read("/pinball/highscores.bin", &blob, sizeof(blob));
	if (rc != EEPFS_ESUCCESS || blob.Magic != HIGHSCORE_MAGIC || blob.Version != SAVE_VERSION || blob.Count != count)
		return false;

	for (int i = 0; i < count; i++)
	{
		std::memcpy(table[i].Name, blob.Scores[i].Name, sizeof(table[i].Name));
		table[i].Name[31] = 0;
		table[i].Score = blob.Scores[i].Score;
	}
	return true;
}

void n64_save::SaveHighScores(const high_score_struct* table, int count)
{
	if (!SaveAvailable || !table || count <= 0 || count > 5)
		return;

	N64HighScoreBlob blob{};
	blob.Magic = HIGHSCORE_MAGIC;
	blob.Version = SAVE_VERSION;
	blob.Count = static_cast<uint16_t>(count);
	for (int i = 0; i < count; i++)
	{
		std::memcpy(blob.Scores[i].Name, table[i].Name, sizeof(blob.Scores[i].Name));
		blob.Scores[i].Name[31] = 0;
		blob.Scores[i].Score = table[i].Score;
	}

	eepfs_write("/pinball/highscores.bin", &blob, sizeof(blob));
}
