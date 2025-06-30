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

#include "../../lib/CConfigHandler.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/ResourceSet.h"
#include "../../lib/GameLibrary.h"

CResDataBar::CResDataBar(const ImagePath & imageName, const Point & position)
{
	pos.x += position.x;
	pos.y += position.y;

	OBJECT_CONSTRUCTION;
	background = std::make_shared<CPicture>(imageName, 0, 0);
	background->setPlayerColor(GAME->interface()->playerID);

	pos.w = background->pos.w;
	pos.h = background->pos.h;
	
	// Set up accessibility info
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("resource bar")
		.withName("Resource Bar")
		.withDescription("Displays current player resources and date"));
	
	// Enable hover events for accessibility
	addUsedEvents(HOVER);
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

	//TODO: all this should be labels, but they require proper text update on change
	for (auto & entry : resourcePositions)
	{
		std::string text = std::to_string(GAME->interface()->cb->getResourceAmount(entry.first));

		to.drawText(pos.topLeft() + entry.second, FONT_SMALL, Colors::WHITE, ETextAlignment::TOPLEFT, text);
	}

	if (datePosition)
		to.drawText(pos.topLeft() + *datePosition, FONT_SMALL, Colors::WHITE, ETextAlignment::TOPLEFT, buildDateString());
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
