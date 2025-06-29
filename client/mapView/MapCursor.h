/*
 * MapCursor.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/int3.h"
#include "../../lib/Color.h"
#include "../../lib/Point.h"

VCMI_LIB_NAMESPACE_BEGIN
class CGObjectInstance;
VCMI_LIB_NAMESPACE_END

class MapView;
class MapViewModel;
class Canvas;
class IImage;

/// Class representing a keyboard-controlled cursor for map exploration
class MapCursor
{
private:
	MapView & owner;
	std::shared_ptr<MapViewModel> model;
	
	/// Current cursor position in map tiles
	int3 cursorPosition;
	
	/// Whether the cursor is currently active/visible
	bool active;
	
	/// Timer for cursor blinking animation
	uint32_t blinkTimer;
	
	/// Current blink state
	bool blinkState;
	
	/// Cursor outline color
	ColorRGBA cursorColor;
	
	/// Secondary color for blinking effect
	ColorRGBA blinkColor;
	
	/// Check if cursor position is valid on the current map
	bool isValidPosition(const int3 & pos) const;
	
	/// Get object at cursor position, if any
	const CGObjectInstance* getObjectAtCursor() const;
	
	/// Announce what's under cursor for screen readers
	void announcePosition() const;
	
public:
	MapCursor(MapView & owner, const std::shared_ptr<MapViewModel> & model);
	
	/// Move cursor by specified offset
	void moveCursor(const Point & direction);
	
	/// Move cursor to specific position
	void setCursorPosition(const int3 & pos);
	
	/// Get current cursor position
	const int3& getCursorPosition() const { return cursorPosition; }
	
	/// Activate/deactivate cursor
	void setActive(bool isActive);
	
	/// Check if cursor is active
	bool isActive() const { return active; }
	
	/// Update cursor animation
	void tick(uint32_t msPassed);
	
	/// Render cursor on the map
	void render(Canvas & target) const;
	
	/// Handle interaction at cursor position (Enter/Space key)
	void interact();
	
	/// Center view on cursor if it's near edge
	void ensureCursorVisible();
};