#pragma once

class Sound
{
public:
	static bool Init(int channels, bool enableFlag);
	static void Enable(bool enableFlag);
	static void Activate();
	static void Deactivate();
	static void Close();
	static void PlaySound(uint8_t* buf, int time, int size, int samplerate);
	static uint8_t* LoadWaveFile(const std::string& lpName);
	static void FreeSound(uint8_t* buf);
	static void SetChannels(int channels);
	static void SetMusicChannelsUsed(int channels);
	static bool QuerySoundInfo(uint8_t* buf, int* sampleCount, int* sampleRate, float* durationSec);
private:
	static int num_channels;
	static bool enabled_flag;
	static int sfx_channel_start;
	static int sfx_channel_count;
	static int next_sfx_channel;
};
