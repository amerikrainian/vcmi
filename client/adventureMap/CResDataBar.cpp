/*
 * CResDataBar.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "CResDataBar.h"

#include "../CPlayerInterface.h"
#include "../render/Canvas.h"
#include "../render/Colors.h"
#include "../render/EFont.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/TextAlignment.h"
#include "../widgets/Images.h"
#include "../render/IRenderHandler.h"
#include "../render/IFont.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/ResourceSet.h"
#include "../../lib/GameLibrary.h"
#include "../gui/EventDispatcher.h"
#include "../gui/Shortcut.h"

CResDataBar::CResDataBar(const ImagePath & imageName, const Point & position)
{
	pos.x += position.x;
	pos.y += position.y;

	OBJECT_CONSTRUCTION;
	background = std::make_shared<CPicture>(imageName, 0, 0);
	background->setPlayerColor(GAME->interface()->playerID);

	pos.w = background->pos.w;
	pos.h = background->pos.h;
	
	// Set up accessibility info with tab order
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("resource bar")
		.withName("Resource Bar")
		.withDescription("Displays current player resources and date")
		.withTabOrder(90)); // Resource bar comes after main buttons (50-61) and town/hero lists (70-71)
	
	// Enable hover and keyboard events for accessibility
	addUsedEvents(HOVER | KEYBOARD);
}

CResDataBar::CResDataBar(const ImagePath & defname, int x, int y, int offx, int offy, int resdist, int datedist):
	CResDataBar(defname, Point(x,y))
{
	for (int i = 0; i < 7 ; i++)
		resourcePositions[GameResID(i)] = Point( offx + resdist*i, offy );

	datePosition = resourcePositions[GameResID::GOLD] + Point(datedist, 0);
}

void CResDataBar::setDatePosition(const Point & position)
{
	datePosition = position;
}

void CResDataBar::setResourcePosition(const GameResID & resource, const Point & position)
{
	resourcePositions[resource] = position;
}

std::string CResDataBar::buildDateString()
{
	std::string pattern = "%s: %d, %s: %d, %s: %d";

	auto formatted = boost::format(pattern)
		% LIBRARY->generaltexth->translate("core.genrltxt.62") % GAME->interface()->cb->getDate(Date::MONTH)
		% LIBRARY->generaltexth->translate("core.genrltxt.63") % GAME->interface()->cb->getDate(Date::WEEK)
		% LIBRARY->generaltexth->translate("core.genrltxt.64") % GAME->interface()->cb->getDate(Date::DAY_OF_WEEK);

	return boost::str(formatted);
}

void CResDataBar::showAll(Canvas & to)
{
	CIntObject::showAll(to);

	// Resource order for navigation
	const std::vector<GameResID> resourceOrder = {
		GameResID(GameResID::WOOD), GameResID(GameResID::MERCURY), GameResID(GameResID::ORE),
		GameResID(GameResID::SULFUR), GameResID(GameResID::CRYSTAL), GameResID(GameResID::GEMS),
		GameResID(GameResID::GOLD)
	};

	//TODO: all this should be labels, but they require proper text update on change
	int index = 0;
	for (auto & entry : resourcePositions)
	{
		std::string text = std::to_string(GAME->interface()->cb->getResourceAmount(entry.first));
		Point textPos = pos.topLeft() + entry.second;
		
		// Draw focus indicator if this resource is focused
		if (navigationActive && focusedResourceIndex == index)
		{
			// Draw a yellow rectangle around the resource value
			auto font = ENGINE->renderHandler().loadFont(FONT_SMALL);
			int textWidth = font->getStringWidth(text);
			int textHeight = font->getLineHeight();
			Rect focusRect(textPos.x - 2, textPos.y - 2, textWidth + 4, textHeight + 4);
			to.drawBorder(focusRect, Colors::YELLOW, 2);
		}

		to.drawText(textPos, FONT_SMALL, Colors::WHITE, ETextAlignment::TOPLEFT, text);
		index++;
	}

	if (datePosition)
	{
		std::string dateText = buildDateString();
		Point textPos = pos.topLeft() + *datePosition;
		
		// Draw focus indicator if date is focused
		if (navigationActive && focusedResourceIndex == 7)
		{
			// Draw a yellow rectangle around the date
			auto font = ENGINE->renderHandler().loadFont(FONT_SMALL);
			int textWidth = font->getStringWidth(dateText);
			int textHeight = font->getLineHeight();
			Rect focusRect(textPos.x - 2, textPos.y - 2, textWidth + 4, textHeight + 4);
			to.drawBorder(focusRect, Colors::YELLOW, 2);
		}
		
		to.drawText(textPos, FONT_SMALL, Colors::WHITE, ETextAlignment::TOPLEFT, dateText);
	}
}

void CResDataBar::setPlayerColor(PlayerColor player)
{
	background->setPlayerColor(player);
}

void CResDataBar::hover(bool on)
{
	if (on && AccessibilityManager::getInstance().isScreenReaderEnabled())
	{
		// Build resource announcement string
		std::vector<std::string> resourceStrings;
		
		// Order of resources: Wood, Mercury, Ore, Sulfur, Crystal, Gems, Gold
		const std::vector<std::pair<GameResID, std::string>> resourceNames = {
			{GameResID(GameResID::WOOD), "Wood"},
			{GameResID(GameResID::MERCURY), "Mercury"},
			{GameResID(GameResID::ORE), "Ore"},
			{GameResID(GameResID::SULFUR), "Sulfur"},
			{GameResID(GameResID::CRYSTAL), "Crystal"},
			{GameResID(GameResID::GEMS), "Gems"},
			{GameResID(GameResID::GOLD), "Gold"}
		};
		
		for (const auto & [resId, resName] : resourceNames)
		{
			int amount = GAME->interface()->cb->getResourceAmount(resId);
			resourceStrings.push_back(resName + ": " + std::to_string(amount));
		}
		
		// Join all resources with commas
		std::string announcement = "Resources: ";
		for (size_t i = 0; i < resourceStrings.size(); ++i)
		{
			announcement += resourceStrings[i];
			if (i < resourceStrings.size() - 1)
				announcement += ", ";
		}
		
		// Also announce the date
		if (datePosition)
		{
			announcement += ". " + buildDateString();
		}
		
		AccessibilityManager::getInstance().announce(announcement);
	}
}

void CResDataBar::keyPressed(EShortcut key) 
{
	// Resource order for navigation
	const std::vector<std::pair<GameResID, std::string>> resourceOrder = {
		{GameResID(GameResID::WOOD), "Wood"},
		{GameResID(GameResID::MERCURY), "Mercury"},
		{GameResID(GameResID::ORE), "Ore"},
		{GameResID(GameResID::SULFUR), "Sulfur"},
		{GameResID(GameResID::CRYSTAL), "Crystal"},
		{GameResID(GameResID::GEMS), "Gems"},
		{GameResID(GameResID::GOLD), "Gold"}
	};
	
	// Check if we should handle navigation
	if (!hasFocus())
		return;
		
	bool handled = false;
	
	switch(key)
	{
		case EShortcut::MOVE_LEFT:
		{
			// Auto-enter navigation mode on first arrow press
			if (!navigationActive)
			{
				navigationActive = true;
				focusedResourceIndex = 0;
			}
			
			if (focusedResourceIndex > 0)
				focusedResourceIndex--;
			else if (focusedResourceIndex == 0 && datePosition)
				focusedResourceIndex = 7; // Move to date (last item)
			else
				focusedResourceIndex = resourceOrder.size() - 1; // Wrap to last resource
			handled = true;
			break;
		}
		
		case EShortcut::MOVE_RIGHT:
		{
			// Auto-enter navigation mode on first arrow press
			if (!navigationActive)
			{
				navigationActive = true;
				focusedResourceIndex = 0;
			}
			
			if (focusedResourceIndex < static_cast<int>(resourceOrder.size()) - 1)
				focusedResourceIndex++;
			else if (focusedResourceIndex == static_cast<int>(resourceOrder.size()) - 1 && datePosition)
				focusedResourceIndex = 7; // Move to date
			else
				focusedResourceIndex = 0; // Wrap to first resource
			handled = true;
			break;
		}
		
		case EShortcut::MOVE_FIRST:
		{
			if (navigationActive)
			{
				focusedResourceIndex = 0;
				handled = true;
			}
			break;
		}
		
		case EShortcut::MOVE_LAST:
		{
			if (navigationActive)
			{
				focusedResourceIndex = datePosition ? 7 : resourceOrder.size() - 1;
				handled = true;
			}
			break;
		}
		
		case EShortcut::GLOBAL_ACCEPT:
		{
			// Exit navigation mode on Enter
			if (navigationActive)
			{
				navigationActive = false;
				focusedResourceIndex = -1;
				handled = true;
			}
			break;
		}
		
		case EShortcut::GLOBAL_CANCEL:
		{
			// Exit navigation mode on Escape
			if (navigationActive)
			{
				navigationActive = false;
				focusedResourceIndex = -1;
				AccessibilityManager::getInstance().announce("Exited resource navigation");
				handled = true;
			}
			break;
		}
		
		case EShortcut::GLOBAL_MOVE_FOCUS:
		{
			// Tab key - announce all resources when focused
			if (AccessibilityManager::getInstance().isScreenReaderEnabled())
			{
				// Build resource announcement string
				std::vector<std::string> resourceStrings;
				
				for (const auto & [resId, resName] : resourceOrder)
				{
					int amount = GAME->interface()->cb->getResourceAmount(resId);
					resourceStrings.push_back(resName + ": " + std::to_string(amount));
				}
				
				// Join all resources with commas
				std::string announcement = "Resources: ";
				for (size_t i = 0; i < resourceStrings.size(); ++i)
				{
					announcement += resourceStrings[i];
					if (i < resourceStrings.size() - 1)
						announcement += ", ";
				}
				
				// Also announce the date
				if (datePosition)
				{
					announcement += ". " + buildDateString();
				}
				
				// Add navigation hint
				announcement += ". Press arrow keys to navigate individual resources";
				
				AccessibilityManager::getInstance().announce(announcement, true);
			}
			break;
		}
	}
	
	// If we handled navigation, announce the focused item
	if (handled && navigationActive && AccessibilityManager::getInstance().isScreenReaderEnabled())
	{
		std::string announcement;
		
		if (focusedResourceIndex >= 0 && focusedResourceIndex < static_cast<int>(resourceOrder.size()))
		{
			// Announce individual resource
			auto [resId, resName] = resourceOrder[focusedResourceIndex];
			int amount = GAME->interface()->cb->getResourceAmount(resId);
			announcement = resName + ": " + std::to_string(amount);
		}
		else if (focusedResourceIndex == 7 && datePosition)
		{
			// Announce date
			announcement = buildDateString();
		}
		
		AccessibilityManager::getInstance().announce(announcement, true);
	}
	
	// Trigger redraw to show focus indicator
	if (handled)
		redraw();
}

bool CResDataBar::captureThisKey(EShortcut key)
{
	// Only capture keys when we have focus
	if (!hasFocus())
		return false;
	
	// Always capture arrow keys to prevent hero movement when focused
	switch(key)
	{
		case EShortcut::MOVE_UP:
		case EShortcut::MOVE_DOWN:
		case EShortcut::MOVE_LEFT:
		case EShortcut::MOVE_RIGHT:
			return true; // Always capture arrow keys when focused
		case EShortcut::MOVE_FIRST:
		case EShortcut::MOVE_LAST:
		case EShortcut::GLOBAL_ACCEPT:
		case EShortcut::GLOBAL_CANCEL:
			return navigationActive; // Only capture when navigating
		default:
			return false; // Let other keys propagate normally
	}
}

void CResDataBar::onFocusLost()
{
	// Reset navigation state when losing focus
	if (navigationActive)
	{
		navigationActive = false;
		focusedResourceIndex = -1;
		redraw();
	}
}
