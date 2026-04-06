#pragma once

class n64_rumble
{
public:
	static void Initialize();
	static void Update();
	static void Shutdown();

	static void PulseFrames(int frames);
	static void PulseFromSound(float durationSec);
};
