#pragma once

#include <string>

class n64_graphics
{
public:
    static void Initialize();
    static void SwapBuffers();
	static void ReleaseStartupAssets();

	static void ShowSplash(std::string text);

	static void UpdateFull();
	static void Update();
};
