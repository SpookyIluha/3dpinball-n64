#include "n64_graphics.h"
#include "pch.h"
#include "render.h"

namespace
{
	surface_t* CurrentSurface = nullptr;
	uint16_t* SplashRaw = nullptr;
	surface_t Offscreen = {};
	bool OffscreenValid = false;
	bool SplashActive = false;

	inline uint16_t PackRgba5551(const Rgba& c)
	{
		// libdragon DEPTH_16_BPP framebuffers are RGBA5551.
		// Source ColorRgba channel layout in this codebase is effectively BGRA for file-derived data,
		// so swap R/B at the final pack stage for correct output on N64.
		const uint16_t a = 1;
		return static_cast<uint16_t>(
			((c.Blue >> 3) << 11) |
			((c.Green >> 3) << 6) |
			((c.Red >> 3) << 1) |
			a);
	}

	void EnsureOffscreen()
	{
		if (OffscreenValid || !render::vscreen)
			return;

		Offscreen = surface_alloc(FMT_RGBA16, render::vscreen->Width, render::vscreen->Height);
		OffscreenValid = Offscreen.buffer != nullptr;
		debugf("n64_graphics: offscreen %dx%d allocated=%d\n",
			render::vscreen->Width, render::vscreen->Height, OffscreenValid ? 1 : 0);
	}

	void LoadSplashRaw()
	{
		if (SplashRaw)
			return;

		auto file = fopen("rom:/splash.raw", "rb");
		if (!file)
			return;

		SplashRaw = new uint16_t[320 * 222];
		fread(SplashRaw, sizeof(uint16_t), 320 * 222, file);
		fclose(file);
	}

	void BlitRegion(int srcX0, int srcY0, int srcW, int srcH)
	{
		if (!render::vscreen)
			return;
		EnsureOffscreen();
		if (!OffscreenValid)
			return;

		int x0 = std::max(0, srcX0);
		int y0 = std::max(0, srcY0);
		int x1 = std::min(render::vscreen->Width, srcX0 + srcW);
		int y1 = std::min(render::vscreen->Height, srcY0 + srcH);

		for (int y = y0; y < y1; ++y)
		{
			auto src = &render::vscreen->BmpBufPtr1[y * render::vscreen->Width + x0];
			auto dst = reinterpret_cast<uint16_t*>(
				reinterpret_cast<uint8_t*>(Offscreen.buffer) + y * Offscreen.stride
			) + x0;

			for (int x = x0; x < x1; ++x)
			{
				*dst++ = PackRgba5551(src->rgba);
				++src;
			}
		}
	}
}

void n64_graphics::Initialize()
{
	rdpq_init();
	LoadSplashRaw();
	CurrentSurface = display_get();
}

void n64_graphics::SwapBuffers()
{
	if (!CurrentSurface)
		return;

	if (OffscreenValid && !SplashActive)
	{
		const int dstW = static_cast<int>(display_get_width());
		const int dstH = static_cast<int>(display_get_height());
		const int baseX = (dstW - Offscreen.width) / 2;
		const int baseY = (dstH - Offscreen.height) / 2;

		rdpq_attach(CurrentSurface, nullptr);
		rdpq_set_mode_copy(false);
		rdpq_tex_blit(&Offscreen, static_cast<float>(baseX), static_cast<float>(baseY), nullptr);
		rdpq_detach_show();
	}
	else
	{
		display_show(CurrentSurface);
	}

	CurrentSurface = display_get();
}

void n64_graphics::ShowSplash(std::string text)
{
	if (!CurrentSurface)
		return;
	SplashActive = true;

	graphics_fill_screen(CurrentSurface, graphics_make_color(0, 0, 0, 255));
	graphics_set_color(graphics_make_color(255, 255, 255, 255), 0);

	if (SplashRaw)
	{
		const int dstW = static_cast<int>(display_get_width());
		const int dstH = static_cast<int>(display_get_height());
		const int splashX = (dstW - 320) / 2;
		const int splashY = (dstH - 222) / 2;

		for (int y = 0; y < 222; y++)
		{
			auto dst = reinterpret_cast<uint16_t*>(
				reinterpret_cast<uint8_t*>(CurrentSurface->buffer) + (splashY + y) * CurrentSurface->stride
			) + splashX;
			auto src = &SplashRaw[y * 320];
			std::memcpy(dst, src, 320 * sizeof(uint16_t));
		}
	}

	std::vector<std::string> lines;
	size_t start = 0;
	while (start <= text.size())
	{
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
		{
			lines.push_back(text.substr(start));
			break;
		}
		lines.push_back(text.substr(start, end - start));
		start = end + 1;
	}

	int maxChars = 0;
	for (const auto& line : lines)
		maxChars = std::max(maxChars, static_cast<int>(line.size()));

	const int textX = (static_cast<int>(display_get_width()) - maxChars * 8) / 2;
	int textY = static_cast<int>(display_get_height()) / 2 + 120;
	for (const auto& line : lines)
	{
		graphics_draw_text(CurrentSurface, textX, textY, line.c_str());
		textY += 10;
	}
}

void n64_graphics::UpdateFull()
{
	SplashActive = false;
	BlitRegion(0, 0, render::vscreen->Width, render::vscreen->Height);
}

void n64_graphics::Update()
{
	SplashActive = false;
	for (const auto& dirty : render::get_dirty_regions())
	{
		BlitRegion(dirty.XPosition, dirty.YPosition, dirty.Width, dirty.Height);
	}
	render::get_dirty_regions().clear();
}
