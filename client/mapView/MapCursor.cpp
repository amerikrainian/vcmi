/*
 * MapCursor.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "MapCursor.h"

#include "MapView.h"
#include "MapViewModel.h"
#include "../render/Canvas.h"
#include "../render/IImage.h"
#include "../CPlayerInterface.h"
#include "../adventureMap/AdventureMapInterface.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/AccessibilityManager.h"

#include "../../lib/callback/CCallback.h"
#include "../../lib/mapObjects/CGObjectInstance.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/TerrainHandler.h"
#include "../../lib/callback/IGameInfoCallback.h"
#include "../../lib/mapping/CMapDefines.h"
#include "../../lib/pathfinder/CGPathNode.h"
#include "../PlayerLocalState.h"


MapCursor::MapCursor(MapView & owner, const std::shared_ptr<MapViewModel> & model)
	: owner(owner)
	, model(model)
	, cursorPosition(0, 0, 0)
	, active(false)
	, blinkTimer(0)
	, blinkState(true)
	, cursorColor(255, 255, 0, 200) // Yellow with transparency
	, blinkColor(255, 128, 0, 200) // Orange for blinking
{
}

bool MapCursor::isValidPosition(const int3 & pos) const
{
	if (!GAME->interface()->cb)
		return false;
		
	const auto & mapSize = GAME->interface()->cb->getMapSize();
	return pos.x >= 0 && pos.x < mapSize.x &&
	       pos.y >= 0 && pos.y < mapSize.y &&
	       pos.z >= 0 && pos.z < mapSize.z;
}

const CGObjectInstance* MapCursor::getObjectAtCursor() const
{
	if (!isValidPosition(cursorPosition))
		return nullptr;
		
	auto objects = GAME->interface()->cb->getVisitableObjs(cursorPosition);
	if (!objects.empty())
		return objects.front();
	
	auto topObj = GAME->interface()->cb->getTopObj(cursorPosition);
	if (topObj)
		return topObj;
		
	return nullptr;
}

void MapCursor::announcePosition() const
{
	if (!isValidPosition(cursorPosition))
		return;
	
	// Don't announce cursor position during hero movement
	// Hero movement announcements are handled by HeroMovementController
	// NOTE: For keyboard cursor movement, we always want to announce regardless of hero movement
	bool isHeroMoving = GAME->interface()->isHeroMoving();
	logGlobal->info("MapCursor::announcePosition - hero moving: %s", isHeroMoving ? "yes" : "no");
	
	// Temporarily disabled to debug cursor announcements
	// if (isHeroMoving)
	//     return;
		
	std::string announcement = "Position " + std::to_string(cursorPosition.x) + ", " + std::to_string(cursorPosition.y);
	
	// Get terrain type
	auto terrain = GAME->interface()->cb->getTile(cursorPosition, false);
	if (terrain && terrain->getTerrain())
	{
		announcement += ", " + terrain->getTerrain()->getNameTranslated();
	}
	
	// Get object at position
	const CGObjectInstance* obj = getObjectAtCursor();
	if (obj)
	{
		announcement += ", " + obj->getObjectName();
	}
	
	// Use AccessibilityManager to announce to screen reader
	AccessibilityManager::getInstance().announce(announcement, false);
	
	// Also log it for debugging
	logGlobal->info("Map cursor: %s", announcement);
}

void MapCursor::moveCursor(const Point & direction)
{
	if (!active)
		return;
		
	int3 newPos = cursorPosition;
	newPos.x += direction.x;
	newPos.y += direction.y;

	if (isValidPosition(newPos))
	{
		// Check if movement is allowed based on hero's path
		const CGHeroInstance* hero = GAME->interface()->localState->getCurrentHero();
		if (hero)
		{
			// Get pathfinding info for the hero
			const CGPathNode* pathNode = GAME->interface()->getPathsInfo(hero)->getPathInfo(newPos);
			
			// Only allow movement if there exists a path to this tile
			// Check if the tile is accessible (not EPathAccessibility::BLOCKED)
			if (pathNode->accessible == EPathAccessibility::BLOCKED)
			{
				AccessibilityManager::getInstance().announce("Cannot move cursor there - no path exists", false);
				return;
			}
		}
		
		cursorPosition = newPos;
		announcePosition();
		ensureCursorVisible();
	}
}

void MapCursor::setCursorPosition(const int3 & pos)
{
	if (isValidPosition(pos))
	{
		cursorPosition = pos;
		if (active)
		{
			announcePosition();
			ensureCursorVisible();
		}
	}
}

void MapCursor::setActive(bool isActive)
{
	active = isActive;
	blinkTimer = 0;
	blinkState = true;
	
	if (active)
	{
		// Set initial position to current hero if available, otherwise view center
		const CGHeroInstance* hero = GAME->interface()->localState->getCurrentHero();
		int3 initialPos;
		
		if (hero)
		{
			// Start cursor on hero position
			initialPos = hero->visitablePos();
			logGlobal->info("MapCursor::setActive - Starting cursor on hero at position (%d, %d)", 
			                initialPos.x, initialPos.y);
		}
		else
		{
			// No hero selected, use view center
			initialPos = model->getTileAtPoint(Point(model->getPixelsVisibleDimensions().x / 2, 
			                                         model->getPixelsVisibleDimensions().y / 2));
			initialPos.z = model->getLevel();
			logGlobal->info("MapCursor::setActive - No hero, starting cursor at view center (%d, %d)", 
			                initialPos.x, initialPos.y);
		}
		
		// Don't announce when just activating - wait for actual movement
		// This prevents duplicate announcements when pressing Ctrl+Arrow
		if (isValidPosition(initialPos))
		{
			cursorPosition = initialPos;
			ensureCursorVisible();
			// Note: announcePosition() is intentionally not called here
		}
	}
}

void MapCursor::tick(uint32_t msPassed)
{
	if (!active)
		return;
		
	blinkTimer += msPassed;
	
	// Blink every 500ms
	if (blinkTimer >= 500)
	{
		blinkTimer = 0;
		blinkState = !blinkState;
	}
}

void MapCursor::render(Canvas & target) const
{
	if (!active || !isValidPosition(cursorPosition))
		return;
		
	// Only render if cursor is on current level
	if (cursorPosition.z != model->getLevel())
		return;
		
	// Get the screen position of the cursor tile
	Rect tileArea = model->getTargetTileArea(cursorPosition);
	
	// Check if tile is visible on screen
	Rect screenArea(Point(0, 0), model->getPixelsVisibleDimensions());
	Rect intersection = screenArea.intersect(tileArea);
	if (intersection.w > 0 && intersection.h > 0)
	{
		// Draw cursor outline
		ColorRGBA color = blinkState ? cursorColor : blinkColor;
		
		// Draw main border with thick lines
		int borderWidth = 3;
		target.drawBorder(tileArea, color, borderWidth);
		
		// Draw corner accents for better visibility
		int accentLength = 8;
		ColorRGBA accentColor(255, 255, 255, 255); // White accents
		
		// Top-left corner
		target.drawLine(Point(tileArea.x, tileArea.y), 
		                Point(tileArea.x + accentLength, tileArea.y), 
		                accentColor, accentColor);
		target.drawLine(Point(tileArea.x, tileArea.y), 
		                Point(tileArea.x, tileArea.y + accentLength), 
		                accentColor, accentColor);
		
		// Top-right corner
		target.drawLine(Point(tileArea.x + tileArea.w - 1, tileArea.y), 
		                Point(tileArea.x + tileArea.w - 1 - accentLength, tileArea.y), 
		                accentColor, accentColor);
		target.drawLine(Point(tileArea.x + tileArea.w - 1, tileArea.y), 
		                Point(tileArea.x + tileArea.w - 1, tileArea.y + accentLength), 
		                accentColor, accentColor);
		
		// Bottom-left corner
		target.drawLine(Point(tileArea.x, tileArea.y + tileArea.h - 1), 
		                Point(tileArea.x + accentLength, tileArea.y + tileArea.h - 1), 
		                accentColor, accentColor);
		target.drawLine(Point(tileArea.x, tileArea.y + tileArea.h - 1), 
		                Point(tileArea.x, tileArea.y + tileArea.h - 1 - accentLength), 
		                accentColor, accentColor);
		
		// Bottom-right corner
		target.drawLine(Point(tileArea.x + tileArea.w - 1, tileArea.y + tileArea.h - 1), 
		                Point(tileArea.x + tileArea.w - 1 - accentLength, tileArea.y + tileArea.h - 1), 
		                accentColor, accentColor);
		target.drawLine(Point(tileArea.x + tileArea.w - 1, tileArea.y + tileArea.h - 1), 
		                Point(tileArea.x + tileArea.w - 1, tileArea.y + tileArea.h - 1 - accentLength), 
		                accentColor, accentColor);
	}
}

void MapCursor::interact()
{
	if (!active || !isValidPosition(cursorPosition))
		return;
		
	// Simulate a left click at the cursor position
	if (adventureInt)
	{
		adventureInt->onTileLeftClicked(cursorPosition);
	}
}

void MapCursor::ensureCursorVisible()
{
	if (!active)
		return;
		
	// Get the screen position of the cursor tile
	Rect tileArea = model->getTargetTileArea(cursorPosition);
	Rect screenArea(Point(0, 0), model->getPixelsVisibleDimensions());
	
	// Calculate margins from screen edges (in pixels)
	const int margin = 64; // 2 tiles at standard zoom
	
	// Check if cursor is too close to edges
	Point newCenter = model->getMapViewCenter();
	bool needsUpdate = false;
	
	if (tileArea.x < margin)
	{
		// Too close to left edge
		newCenter.x -= (margin - tileArea.x);
		needsUpdate = true;
	}
	else if (tileArea.x + tileArea.w > screenArea.w - margin)
	{
		// Too close to right edge
		newCenter.x += (tileArea.x + tileArea.w - (screenArea.w - margin));
		needsUpdate = true;
	}
	
	if (tileArea.y < margin)
	{
		// Too close to top edge
		newCenter.y -= (margin - tileArea.y);
		needsUpdate = true;
	}
	else if (tileArea.y + tileArea.h > screenArea.h - margin)
	{
		// Too close to bottom edge
		newCenter.y += (tileArea.y + tileArea.h - (screenArea.h - margin));
		needsUpdate = true;
	}
	
	if (needsUpdate)
	{
		owner.onMapScrolled(newCenter - model->getMapViewCenter());
	}
}