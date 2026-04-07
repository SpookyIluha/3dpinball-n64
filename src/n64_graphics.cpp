#include "n64_graphics.h"
#include "pch.h"
#include "render.h"

namespace
{
	surface_t* CurrentSurface = nullptr;
	uint16_t* SplashRaw = nullptr;
	bool SplashActive = false;
	bool RdpPresentedFrame = false;
	alignas(8) uint16_t VscreenTlut[256]{};

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

	void DrawVscreenWithRdp()
	{
		if (!render::vscreen || !CurrentSurface)
			return;

		const int dstW = static_cast<int>(display_get_width());
		const int dstH = static_cast<int>(display_get_height());
		const int baseX = (dstW - render::vscreen->Width) / 2;
		const int baseY = (dstH - render::vscreen->Height) / 2;
		const int shakeX = std::clamp(render::get_offset_x(), -12, 12);
		const int shakeY = std::clamp(render::get_offset_y(), -8, 8);
		const int drawX = baseX + shakeX;
		const int drawY = baseY + shakeY;

		const auto* palette = gdrv::Palette5551Table();
		std::memcpy(VscreenTlut, palette, sizeof(VscreenTlut));
		data_cache_hit_writeback(VscreenTlut, sizeof(VscreenTlut));
		data_cache_hit_writeback(render::vscreen->BmpBufPtr1,
			static_cast<unsigned long>(render::vscreen->Stride * render::vscreen->Height));

		surface_t ci8Surface = surface_make(
			render::vscreen->BmpBufPtr1,
			FMT_CI8,
			static_cast<uint16_t>(render::vscreen->Width),
			static_cast<uint16_t>(render::vscreen->Height),
			static_cast<uint16_t>(render::vscreen->Stride));

		rdpq_attach(CurrentSurface, nullptr);
		rdpq_set_mode_fill(RGBA32(0, 0, 0, 0));
		rdpq_fill_rectangle(0, 0, dstW, dstH);
		rdpq_set_mode_copy(false);
		rdpq_mode_tlut(TLUT_RGBA16);
		rdpq_tex_upload_tlut(VscreenTlut, 0, 256);
		rdpq_tex_blit(&ci8Surface, static_cast<float>(drawX), static_cast<float>(drawY), nullptr);
		rdpq_detach_show();
		RdpPresentedFrame = true;
		CurrentSurface = nullptr;
	}
}

void n64_graphics::Initialize()
{
	LoadSplashRaw();
	CurrentSurface = display_get();
}

void n64_graphics::ReleaseStartupAssets()
{
	delete[] SplashRaw;
	SplashRaw = nullptr;
}

void n64_graphics::SwapBuffers()
{
	if (RdpPresentedFrame)
	{
		RdpPresentedFrame = false;
		CurrentSurface = display_get();
		return;
	}

	if (CurrentSurface)
		display_show(CurrentSurface);

	CurrentSurface = display_get();
}

void n64_graphics::ShowSplash(std::string text)
{
	if (!CurrentSurface)
		return;
	SplashActive = true;
	LoadSplashRaw();

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
	DrawVscreenWithRdp();
}

void n64_graphics::Update()
{
	SplashActive = false;
	if (!CurrentSurface || !render::vscreen)
		return;
	DrawVscreenWithRdp();
	render::get_dirty_regions().clear();
}
