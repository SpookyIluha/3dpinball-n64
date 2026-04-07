#include "pch.h"
#include "render.h"

#include "GroupData.h"
#include "options.h"
#include "pb.h"
#include "score.h"
#include "TPinballTable.h"
#include "winmain.h"

std::vector<render_sprite_type_struct*> render::dirty_list, render::sprite_list, render::ball_list;
std::vector<rectangle_type> render::dirty_regions;
zmap_header_type* render::background_zmap;
int render::zmap_offset, render::zmap_offsetY, render::offset_x, render::offset_y;
float render::zscaler, render::zmin, render::zmax;
rectangle_type render::vscreen_rect;
gdrv_bitmap8 *render::vscreen, *render::background_bitmap;
bool render::full_redraw_pending = false;

void render::init(gdrv_bitmap8* bmp, float zMin, float zScaler, int width, int height)
{
	zscaler = zScaler;
	zmin = zMin;
	zmax = 4294967300.0f / zScaler + zMin;
	offset_x = 0;
	offset_y = 0;

	vscreen = new gdrv_bitmap8(width, height, false);
	vscreen_rect.YPosition = 0;
	vscreen_rect.XPosition = 0;
	vscreen_rect.Width = width;
	vscreen_rect.Height = height;
	vscreen->YPosition = 0;
	vscreen->XPosition = 0;

	background_bitmap = bmp;
	full_redraw_pending = true;
	if (bmp)
		gdrv::copy_bitmap(vscreen, width, height, 0, 0, bmp, 0, 0);
	else
		gdrv::fill_bitmap(vscreen, vscreen->Width, vscreen->Height, 0, 0, 0);
}

void render::uninit()
{
	delete vscreen;
	for (auto sprite : sprite_list)
		remove_sprite(sprite, false);
	for (auto ball : ball_list)
		remove_ball(ball, false);
	ball_list.clear();
	dirty_list.clear();
	sprite_list.clear();
	dirty_regions.clear();
}

void render::update()
{
	// The RDP samples render::vscreen during present; wait for prior frame to finish
	// before CPU updates dirty regions in-place.
	rspq_wait();

	std::vector<rectangle_type> repaintQueue;
	repaintQueue.reserve(dirty_list.size() * 2 + ball_list.size() * 2);

	auto enqueue_repaint = [&](const rectangle_type& inputRect)
	{
		auto rect = inputRect;
		if (rect.Width <= 0 || rect.Height <= 0)
			return;

		// Inflate by 1px to avoid tiny cracks when moving sprites/balls update edges.
		rect.XPosition -= 1;
		rect.YPosition -= 1;
		rect.Width += 2;
		rect.Height += 2;

		if (maths::rectangle_clip(&rect, &vscreen_rect, &rect) && rect.Width > 0 && rect.Height > 0)
			repaintQueue.push_back(rect);
	};
	if (full_redraw_pending)
	{
		full_redraw_pending = false;

		// Avoid a single huge z-region allocation on 4MB systems by repainting in tiles.
		constexpr int TileW = 96;
		constexpr int TileH = 64;
		std::vector<render_sprite_type_struct*> tileSprites;
		for (int y = 0; y < vscreen_rect.Height; y += TileH)
		{
			for (int x = 0; x < vscreen_rect.Width; x += TileW)
			{
				rectangle_type tile{};
				tile.XPosition = x;
				tile.YPosition = y;
				tile.Width = std::min(TileW, vscreen_rect.Width - x);
				tile.Height = std::min(TileH, vscreen_rect.Height - y);

				collect_sprites_for_rect(tile, tileSprites);
				repaint(tile, tileSprites);
				dirty_regions.push_back(tile);
			}
		}
	}

	// Clip dirty sprites with vScreen, collect clipping rectangles.
	for (auto curSprite : dirty_list)
	{
		bool collectRect = false;
		switch (curSprite->VisualType)
		{
		case VisualTypes::Sprite:
			if (curSprite->DirtyRectPrev.Width > 0)
				maths::enclosing_box(&curSprite->DirtyRectPrev, &curSprite->BmpRect, &curSprite->DirtyRect);

			if (maths::rectangle_clip(&curSprite->DirtyRect, &vscreen_rect, &curSprite->DirtyRect))
				collectRect = true;
			else
				curSprite->DirtyRect.Width = -1;
			break;
		case VisualTypes::None:
			if (maths::rectangle_clip(&curSprite->BmpRect, &vscreen_rect, &curSprite->DirtyRect))
				collectRect = !curSprite->Bmp;
			else
				curSprite->DirtyRect.Width = -1;
			break;
		default: break;
		}

		if (collectRect && curSprite->DirtyRect.Width > 0 && curSprite->DirtyRect.Height > 0)
		{
			enqueue_repaint(curSprite->DirtyRect);
		}
	}

	// Balls are always dynamic: repaint previous and current locations.
	for (auto ball : ball_list)
	{
		rectangle_type clipped{};
		if (ball->DirtyRectPrev.Width > 0 && maths::rectangle_clip(&ball->DirtyRectPrev, &vscreen_rect, &clipped))
			enqueue_repaint(clipped);

		if (ball->Bmp && maths::rectangle_clip(&ball->BmpRect, &vscreen_rect, &ball->DirtyRect))
			enqueue_repaint(ball->DirtyRect);
		else
			ball->DirtyRect.Width = -1;
	}

	std::vector<render_sprite_type_struct*> regionSprites;
	for (const auto& rect : repaintQueue)
	{
		if (rect.Width <= 0 || rect.Height <= 0)
			continue;

		collect_sprites_for_rect(rect, regionSprites);
		repaint(rect, regionSprites);
		dirty_regions.push_back(rect);
	}

	paint_balls();

	// In the original, this used to blit dirty sprites and balls.
	for (auto sprite : dirty_list)
	{
		sprite->DirtyRectPrev = sprite->DirtyRect;
		if (sprite->UnknownFlag != 0)
			remove_sprite(sprite, true);
	}
	dirty_list.clear();

	for (auto ball : ball_list)
	{
		ball->DirtyRectPrev = ball->DirtyRect;
	}
}

void render::sprite_modified(render_sprite_type_struct* sprite)
{
	if (sprite->VisualType != VisualTypes::Ball && dirty_list.size() < 999)
		dirty_list.push_back(sprite);
}

render_sprite_type_struct* render::create_sprite(VisualTypes visualType, gdrv_bitmap8* bmp, zmap_header_type* zMap,
                                                 int xPosition, int yPosition, rectangle_type* rect)
{
	auto sprite = new render_sprite_type_struct();
	if (!sprite)
		return nullptr;
	sprite->BmpRect.YPosition = yPosition;
	sprite->BmpRect.XPosition = xPosition;
	sprite->Bmp = bmp;
	sprite->VisualType = visualType;
	sprite->UnknownFlag = 0;
	sprite->SpriteArray = nullptr;
	sprite->DirtyRect = rectangle_type{};
	if (rect)
	{
		sprite->BoundingRect = *rect;
	}
	else
	{
		sprite->BoundingRect.Width = -1;
		sprite->BoundingRect.Height = -1;
		sprite->BoundingRect.XPosition = 0;
		sprite->BoundingRect.YPosition = 0;
	}
	if (bmp)
	{
		sprite->BmpRect.Width = bmp->Width;
		sprite->BmpRect.Height = bmp->Height;
	}
	else
	{
		sprite->BmpRect.Width = 0;
		sprite->BmpRect.Height = 0;
	}
	sprite->ZMap = zMap;
	sprite->ZMapOffestX = 0;
	sprite->ZMapOffestY = 0;
	if (!zMap && visualType != VisualTypes::Ball)
	{
		sprite->ZMap = background_zmap;
		sprite->ZMapOffestY = xPosition - zmap_offset;
		sprite->ZMapOffestX = yPosition - zmap_offsetY;
	}
	sprite->DirtyRectPrev = sprite->BmpRect;
	if (visualType == VisualTypes::Ball)
	{
		ball_list.push_back(sprite);
	}
	else
	{
		sprite_list.push_back(sprite);
		sprite_modified(sprite);
	}
	return sprite;
}

void render::remove_sprite(render_sprite_type_struct* sprite, bool removeFromList)
{
	if (removeFromList)
	{
		auto it = std::find(sprite_list.begin(), sprite_list.end(), sprite);
		if (it != sprite_list.end())
			sprite_list.erase(it);
	}

	delete sprite->SpriteArray;
	delete sprite;
}

void render::remove_ball(render_sprite_type_struct* ball, bool removeFromList)
{
	if (removeFromList)
	{
		auto it = std::find(ball_list.begin(), ball_list.end(), ball);
		if (it != ball_list.end())
			ball_list.erase(it);
	}

	delete ball->SpriteArray;
	delete ball;
}

void render::sprite_set(render_sprite_type_struct* sprite, gdrv_bitmap8* bmp, zmap_header_type* zMap, int xPos,
                        int yPos)
{
	if (sprite)
	{
		sprite->BmpRect.XPosition = xPos;
		sprite->BmpRect.YPosition = yPos;
		sprite->Bmp = bmp;
		if (bmp)
		{
			sprite->BmpRect.Width = bmp->Width;
			sprite->BmpRect.Height = bmp->Height;
		}
		sprite->ZMap = zMap;
		sprite_modified(sprite);
	}
}

void render::sprite_set_bitmap(render_sprite_type_struct* sprite, gdrv_bitmap8* bmp)
{
	if (sprite && sprite->Bmp != bmp)
	{
		sprite->Bmp = bmp;
		if (bmp)
		{
			sprite->BmpRect.Width = bmp->Width;
			sprite->BmpRect.Height = bmp->Height;
		}
		sprite_modified(sprite);
	}
}

void render::set_background_zmap(struct zmap_header_type* zMap, int offsetX, int offsetY)
{
	background_zmap = zMap;
	zmap_offset = offsetX;
	zmap_offsetY = offsetY;
}

void render::ball_set(render_sprite_type_struct* sprite, gdrv_bitmap8* bmp, float depth, int xPos, int yPos)
{
	if (sprite)
	{
		sprite->Bmp = bmp;
		if (bmp)
		{
			sprite->BmpRect.XPosition = xPos;
			sprite->BmpRect.YPosition = yPos;
			sprite->BmpRect.Width = bmp->Width;
			sprite->BmpRect.Height = bmp->Height;
		}
		if (depth >= zmin)
		{
			float depth2 = (depth - zmin) * zscaler;
			if (depth2 <= zmax)
				sprite->Depth = static_cast<short>(depth2);
			else
				sprite->Depth = -1;
		}
		else
		{
			sprite->Depth = 0;
		}
	}
}

void render::collect_sprites_for_rect(const rectangle_type& rect, std::vector<render_sprite_type_struct*>& outSprites)
{
	outSprites.clear();
	auto region = rect;
	for (auto sprite : sprite_list)
	{
		if (sprite->UnknownFlag || !sprite->Bmp)
			continue;

		if (maths::rectangle_clip(&sprite->BmpRect, &region, nullptr))
			outSprites.push_back(sprite);
	}
}

void render::repaint(const rectangle_type& rect, const std::vector<render_sprite_type_struct*>& sprites)
{
	if (rect.Width <= 0 || rect.Height <= 0)
		return;

	// Restore from static table background. Handle background bitmaps with non-zero placement.
	gdrv::fill_bitmap(vscreen, rect.Width, rect.Height, rect.XPosition, rect.YPosition, 0);
	if (background_bitmap)
	{
		auto region = rect;
		rectangle_type bgRect
		{
			background_bitmap->XPosition,
			background_bitmap->YPosition,
			background_bitmap->Width,
			background_bitmap->Height
		};
		rectangle_type overlap{};
		if (maths::rectangle_clip(&region, &bgRect, &overlap))
		{
			const int srcX = overlap.XPosition - background_bitmap->XPosition;
			const int srcY = overlap.YPosition - background_bitmap->YPosition;
			gdrv::copy_bitmap(vscreen, overlap.Width, overlap.Height,
			                  overlap.XPosition, overlap.YPosition,
			                  background_bitmap, srcX, srcY);
		}
	}

	auto regionZ = new zmap_header_type(rect.Width, rect.Height, rect.Width);
	zdrv::fill(regionZ, rect.Width, rect.Height, 0, 0, 0xFFFF);
	auto region = rect;

	rectangle_type clipRect{};
	for (auto refSprite : sprites)
	{
		if (maths::rectangle_clip(&refSprite->BmpRect, &region, &clipRect))
		{
			zdrv::paint(
				clipRect.Width,
				clipRect.Height,
				vscreen,
				clipRect.XPosition,
				clipRect.YPosition,
				regionZ,
				clipRect.XPosition - rect.XPosition,
				clipRect.YPosition - rect.YPosition,
				refSprite->Bmp,
				clipRect.XPosition - refSprite->BmpRect.XPosition,
				clipRect.YPosition - refSprite->BmpRect.YPosition,
				refSprite->ZMap,
				clipRect.XPosition + refSprite->ZMapOffestY - refSprite->BmpRect.XPosition,
				clipRect.YPosition + refSprite->ZMapOffestX - refSprite->BmpRect.YPosition);
		}
	}

	delete regionZ;
}

void render::build_z_for_region(const rectangle_type& rect, const std::vector<render_sprite_type_struct*>& sprites,
                                zmap_header_type* zMap)
{
	zdrv::fill(zMap, rect.Width, rect.Height, 0, 0, 0xFFFF);
	auto region = rect;

	rectangle_type clipRect{};
	for (auto refSprite : sprites)
	{
		if (maths::rectangle_clip(&refSprite->BmpRect, &region, &clipRect))
		{
			zdrv::paint_depth_masked(
				clipRect.Width,
				clipRect.Height,
				zMap,
				clipRect.XPosition - rect.XPosition,
				clipRect.YPosition - rect.YPosition,
				refSprite->ZMap,
				clipRect.XPosition + refSprite->ZMapOffestY - refSprite->BmpRect.XPosition,
				clipRect.YPosition + refSprite->ZMapOffestX - refSprite->BmpRect.YPosition,
				refSprite->Bmp,
				clipRect.XPosition - refSprite->BmpRect.XPosition,
				clipRect.YPosition - refSprite->BmpRect.YPosition);
		}
	}
}

void render::paint_balls()
{
	// Sort ball sprites by depth
	for (auto i = 0u; i < ball_list.size(); i++)
	{
		for (auto j = i; j < ball_list.size() / 2; ++j)
		{
			auto ballA = ball_list[j];
			auto ballB = ball_list[i];
			if (ballB->Depth > ballA->Depth)
			{
				ball_list[i] = ballA;
				ball_list[j] = ballB;
			}
		}
	}

	rectangle_type ballsUnion{};
	ballsUnion.Width = -1;
	for (auto ball : ball_list)
	{
		if (ball->DirtyRect.Width > 0)
		{
			if (ballsUnion.Width <= 0)
				ballsUnion = ball->DirtyRect;
			else
				maths::enclosing_box(&ballsUnion, &ball->DirtyRect, &ballsUnion);
		}
	}
	if (ballsUnion.Width <= 0 || ballsUnion.Height <= 0)
		return;

	std::vector<render_sprite_type_struct*> unionSprites;
	collect_sprites_for_rect(ballsUnion, unionSprites);
	auto unionZ = new zmap_header_type(ballsUnion.Width, ballsUnion.Height, ballsUnion.Width);
	build_z_for_region(ballsUnion, unionSprites, unionZ);

	for (auto ball : ball_list)
	{
		auto dirty = &ball->DirtyRect;
		if (dirty->Width > 0 && ball->Bmp)
		{
			zdrv::paint_flat(
				dirty->Width,
				dirty->Height,
				vscreen,
				dirty->XPosition,
				dirty->YPosition,
				unionZ,
				dirty->XPosition - ballsUnion.XPosition,
				dirty->YPosition - ballsUnion.YPosition,
				ball->Bmp,
				dirty->XPosition - ball->BmpRect.XPosition,
				dirty->YPosition - ball->BmpRect.YPosition,
				ball->Depth);

			dirty_regions.push_back(*dirty);
		}
	}

	delete unionZ;
}

void render::unpaint_balls()
{
}

void render::shift(int offsetX, int offsetY)
{
	offset_x += offsetX;
	offset_y += offsetY;
}

int render::get_offset_x()
{
	return offset_x;
}

int render::get_offset_y()
{
	return offset_y;
}

void render::build_occlude_list()
{
	std::vector<render_sprite_type_struct*>* spriteArr = nullptr;
	for (auto mainSprite : sprite_list)
	{
		if (mainSprite->SpriteArray)
		{
			delete mainSprite->SpriteArray;
			mainSprite->SpriteArray = nullptr;
		}

		if (!mainSprite->UnknownFlag && mainSprite->BoundingRect.Width != -1)
		{
			if (!spriteArr)
				spriteArr = new std::vector<render_sprite_type_struct*>();

			for (auto refSprite : sprite_list)
			{
				if (!refSprite->UnknownFlag
					&& refSprite->BoundingRect.Width != -1
					&& maths::rectangle_clip(&mainSprite->BoundingRect, &refSprite->BoundingRect, nullptr)
					&& spriteArr)
				{
					spriteArr->push_back(refSprite);
				}
			}

			if (mainSprite->Bmp && spriteArr->size() < 2)
				spriteArr->clear();
			if (!spriteArr->empty())
			{
				mainSprite->SpriteArray = spriteArr;
				spriteArr = nullptr;
			}
		}
	}

	delete spriteArr;
}

void render::invalidate_all()
{
	full_redraw_pending = true;
}
