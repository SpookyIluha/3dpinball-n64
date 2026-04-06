#include "pch.h"
#include "Sound.h"

namespace
{
	constexpr int MaxMixerChannels = 32;
	constexpr int DefaultMusicChannels = 16;
	constexpr float SfxVolume = 0.30f;

	struct WavHandle
	{
		std::string Path;
		int SampleCount;
		int SampleRate;
		float DurationSec;
	};

	struct ChannelState
	{
		wav64_t* Wave;
	};

	ChannelState ChannelStates[MaxMixerChannels]{};
}

int Sound::num_channels = 0;
bool Sound::enabled_flag = false;
int Sound::sfx_channel_start = DefaultMusicChannels;
int Sound::sfx_channel_count = 1;
int Sound::next_sfx_channel = 0;

bool Sound::Init(int channels, bool enableFlag)
{
	Enable(enableFlag);
	SetChannels(channels);
	return true;
}

void Sound::Enable(bool enableFlag)
{
	enabled_flag = enableFlag;
}

void Sound::Activate()
{
	enabled_flag = true;
}

void Sound::Deactivate()
{
	enabled_flag = false;
}

void Sound::Close()
{
	for (int i = 0; i < MaxMixerChannels; i++)
	{
		if (ChannelStates[i].Wave)
		{
			wav64_close(ChannelStates[i].Wave);
			ChannelStates[i].Wave = nullptr;
		}
	}
	enabled_flag = false;
}

void Sound::SetMusicChannelsUsed(int channels)
{
	sfx_channel_start = std::max(0, std::min(MaxMixerChannels - 1, channels));
	SetChannels(num_channels <= 0 ? 8 : num_channels);
}

void Sound::SetChannels(int channels)
{
	if (channels <= 0)
		channels = 8;

	num_channels = channels;
	const int available = std::max(1, MaxMixerChannels - sfx_channel_start);
	sfx_channel_count = std::max(1, std::min(channels, available));
	next_sfx_channel = 0;
	for (int i = sfx_channel_start; i < sfx_channel_start + sfx_channel_count; i++)
	{
		mixer_ch_set_vol(i, SfxVolume, SfxVolume);
	}
	debugf("Sound: configured SFX channels start=%d count=%d\n", sfx_channel_start, sfx_channel_count);
}

void Sound::PlaySound(uint8_t* buf, int time, int size, int samplerate)
{
	(void)time;
	(void)size;
	(void)samplerate;
	if (!enabled_flag || !buf)
		return;

	auto* handle = reinterpret_cast<WavHandle*>(buf);
	if (handle->Path.empty())
		return;

	// Free finished one-shot waves to keep heap bounded.
	for (int i = sfx_channel_start; i < sfx_channel_start + sfx_channel_count; i++)
	{
		if (ChannelStates[i].Wave && !mixer_ch_playing(i))
		{
			wav64_close(ChannelStates[i].Wave);
			ChannelStates[i].Wave = nullptr;
		}
	}

	const int channel = sfx_channel_start + next_sfx_channel;
	next_sfx_channel = (next_sfx_channel + 1) % sfx_channel_count;

	if (ChannelStates[channel].Wave)
	{
		wav64_close(ChannelStates[channel].Wave);
		ChannelStates[channel].Wave = nullptr;
	}

	wav64_loadparms_t parms{};
	parms.streaming_mode = WAV64_STREAMING_NONE;

	auto* wave = wav64_load(handle->Path.c_str(), &parms);
	if (!wave)
	{
		debugf("Sound: failed to load SFX for play '%s'\n", handle->Path.c_str());
		return;
	}

	wav64_set_loop(wave, false);
	wav64_play(wave, channel);
	mixer_ch_set_vol(channel, SfxVolume, SfxVolume);
	ChannelStates[channel].Wave = wave;
}

uint8_t* Sound::LoadWaveFile(const std::string& lpName)
{
	auto testFile = fopen(lpName.c_str(), "rb");
	if (!testFile)
	{
		debugf("Sound: missing wav64 '%s'\n", lpName.c_str());
		return nullptr;
	}
	fclose(testFile);

	auto* handle = new WavHandle{};
	handle->Path = lpName;
	handle->SampleCount = 0;
	handle->SampleRate = 0;
	handle->DurationSec = 0.0f;

	// Probe metadata without retaining a long-lived open stream.
	wav64_loadparms_t probeParms{};
	probeParms.streaming_mode = WAV64_STREAMING_FULL;
	auto* probe = wav64_load(lpName.c_str(), &probeParms);
	if (!probe)
	{
		debugf("Sound: wav64_load probe failed '%s'\n", lpName.c_str());
		delete handle;
		return nullptr;
	}
	handle->SampleCount = probe->wave.len;
	handle->SampleRate = static_cast<int>(probe->wave.frequency);
	handle->DurationSec = handle->SampleRate > 0
		? static_cast<float>(handle->SampleCount / static_cast<double>(handle->SampleRate))
		: 0.0f;
	wav64_close(probe);

	return reinterpret_cast<uint8_t*>(handle);
}

void Sound::FreeSound(uint8_t* wave)
{
	if (!wave)
		return;

	auto* handle = reinterpret_cast<WavHandle*>(wave);
	delete handle;
}

bool Sound::QuerySoundInfo(uint8_t* buf, int* sampleCount, int* sampleRate, float* durationSec)
{
	if (!buf)
		return false;
	auto* handle = reinterpret_cast<WavHandle*>(buf);
	if (handle->Path.empty())
		return false;

	if (sampleCount)
		*sampleCount = handle->SampleCount;
	if (sampleRate)
		*sampleRate = handle->SampleRate;
	if (durationSec)
		*durationSec = handle->DurationSec;
	return true;
}
