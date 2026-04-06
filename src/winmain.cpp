#include "pch.h"
#include "winmain.h"

#include <malloc.h>
#include <cstdlib>

#include "control.h"
#include "n64_graphics.h"
#include "n64_input.h"
#include "n64_rumble.h"
#include "midi.h"
#include "n64_save.h"
#include "options.h"
#include "pb.h"
#include "pinball.h"
#include "render.h"
#include "Sound.h"

int winmain::bQuit = 0;
int winmain::activated;
int winmain::DispFrameRate = 0;
int winmain::DispGRhistory = 0;
int winmain::single_step = 0;
int winmain::last_mouse_x;
int winmain::last_mouse_y;
int winmain::mouse_down;
int winmain::no_time_loss;

std::string winmain::DatFileName;
bool winmain::ShowSpriteViewer = false;
bool winmain::LaunchBallEnabled = true;
bool winmain::HighScoresEnabled = true;
bool winmain::DemoActive = false;
char* winmain::BasePath;
std::string winmain::FpsDetails;
double winmain::UpdateToFrameRatio;
winmain::DurationMs winmain::TargetFrameTime;
optionsStruct& winmain::Options = options::Options;

int winmain::WinMain(LPCSTR lpCmdLine)
{
	(void)lpCmdLine;
	std::set_new_handler(memalloc_failure);
	debugf("winmain: boot start\n");

	// CPU software blit path: avoid VI dedither, which is intended for RDP-dithered output.
	display_init(RESOLUTION_640x480, DEPTH_16_BPP, 3, GAMMA_NONE, FILTERS_DISABLED);
	joypad_init();
	audio_init(44100, 4);
	mixer_init(32);

	if (dfs_init(DFS_DEFAULT_LOCATION) != DFS_ESUCCESS)
	{
		debugf("Failed to init DragonFS (rom:/)\n");
		return 1;
	}
	debugf("winmain: DragonFS initialized\n");

	n64_save::Initialize();
	n64_graphics::Initialize();
	n64_input::Initialize();
	n64_rumble::Initialize();

	BasePath = (char*)"rom:/";
	pinball::quickFlag = 0;
	pb::FullTiltMode = false;
	DatFileName = "PINBALL.DAT";
	debugf("winmain: dat='%s' base='%s'\n", DatFileName.c_str(), BasePath);

	auto pinballDat = fopen(pinball::make_path_name(DatFileName).c_str(), "rb");
	if (!pinballDat)
	{
		PrintFatalError("Could not find %s", pinball::make_path_name(DatFileName).c_str());
	}
	fclose(pinballDat);

	n64_graphics::ShowSplash("Loading 3D Pinball Space Cadet");
	n64_graphics::SwapBuffers();

	options::init();
	if (!Sound::Init(Options.SoundChannels, Options.Sounds))
		Options.Sounds = false;
	debugf("winmain: sound init channels=%d enabled=%d\n", Options.SoundChannels, Options.Sounds ? 1 : 0);

	if (!pinball::quickFlag && !midi::music_init())
		Options.Music = false;
	debugf("winmain: music enabled=%d\n", Options.Music ? 1 : 0);

	if (pb::init())
	{
		PrintFatalError("Could not load game data:\n%s file is missing.\n", pinball::make_path_name(DatFileName).c_str());
	}
	debugf("winmain: pb::init complete\n");

	pb::reset_table();
	pb::firsttime_setup();
	pb::replay_level(0);

	n64_graphics::UpdateFull();
	n64_graphics::SwapBuffers();

	constexpr float FixedDeltaMs = 1000.0f / 60.0f;

	bQuit = false;
	while (!bQuit)
	{
		n64_input::ScanPads();
		n64_rumble::Update();

		if (n64_input::Exit())
			break;
		if (n64_input::Pause())
			pause();
		if (n64_input::NewGame())
			new_game();

		pb::keydown();
		pb::keyup();

		if (!single_step)
			pb::frame(FixedDeltaMs);

		n64_graphics::Update();
		mixer_try_play();
		n64_graphics::SwapBuffers();
	}

	end_pause();
	options::uninit();
	midi::music_shutdown();
	pb::uninit();
	Sound::Close();
	n64_rumble::Shutdown();
	n64_save::Shutdown();
	debugf("winmain: shutdown complete\n");

	return 0;
}

void winmain::memalloc_failure()
{
	midi::music_stop();
	Sound::Close();
	n64_rumble::Shutdown();
	char* caption = pinball::get_rc_string(170, 0);
	char* text = pinball::get_rc_string(179, 0);

	PrintFatalError("%s %s\n", caption, text);
}

void winmain::end_pause()
{
	if (single_step)
	{
		pb::pause_continue();
		no_time_loss = 1;
	}
}

void winmain::new_game()
{
	end_pause();
	pb::replay_level(0);
}

void winmain::pause()
{
	pb::pause_continue();
	no_time_loss = 1;
}

void winmain::UpdateFrameRate()
{
	auto fps = Options.FramesPerSecond, ups = Options.UpdatesPerSecond;
	UpdateToFrameRatio = static_cast<double>(ups) / fps;
	TargetFrameTime = DurationMs(1000.0 / ups);
}

void winmain::PrintFatalError(const char* message, ...)
{
	char buf[256] = {0};

	va_list args;
	va_start(args, message);
	vsnprintf(buf, sizeof(buf), message, args);
	va_end(args);

	strcat(buf, "\nPress A to exit");

	while (true)
	{
		n64_graphics::ShowSplash(buf);
		n64_input::ScanPads();
		n64_rumble::Update();
		if (n64_input::Button1() || n64_input::Exit())
			break;

		mixer_try_play();
		n64_graphics::SwapBuffers();
	}

	n64_rumble::Shutdown();
	n64_save::Shutdown();
	std::abort();
}