#include "pch.h"
#include "midi.h"

#include "Sound.h"

namespace
{
	xm64player_t XmPlayer{};
	bool XmOpened = false;
	bool XmPlaying = false;
}

int midi::music_init()
{
	FILE* testFile = fopen("rom:/PINBALL.xm64", "rb");
	if (!testFile)
	{
		debugf("midi: rom:/PINBALL.xm64 not found\n");
		return false;
	}
	fclose(testFile);

	xm64player_open(&XmPlayer, "rom:/PINBALL.xm64");
	xm64player_set_loop(&XmPlayer, true);
	Sound::SetMusicChannelsUsed(xm64player_num_channels(&XmPlayer));
	XmOpened = true;
	XmPlaying = false;
	return true;
}

int midi::play_pb_theme()
{
	if (!XmOpened)
		return false;
	if (XmPlaying)
		return true;

	xm64player_play(&XmPlayer, 0);
	XmPlaying = true;
	return true;
}

int midi::music_stop()
{
	if (!XmOpened)
		return false;
	if (!XmPlaying)
		return true;

	xm64player_stop(&XmPlayer);
	XmPlaying = false;
	return true;
}

void midi::music_shutdown()
{
	if (!XmOpened)
		return;

	if (XmPlaying)
	{
		xm64player_stop(&XmPlayer);
		XmPlaying = false;
	}
	xm64player_close(&XmPlayer);
	XmOpened = false;
}
