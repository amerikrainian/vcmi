/*
 * CAdventureOptions.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "AdventureOptions.h"

#include "../CPlayerInterface.h"
#include "../PlayerLocalState.h"
#include "../lobby/CCampaignInfoScreen.h"
#include "../lobby/CScenarioInfoScreen.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/WindowHandler.h"
#include "../gui/Shortcut.h"
#include "../gui/AccessibilityManager.h"
#include "../widgets/Buttons.h"

#include "../../lib/GameLibrary.h"
#include "../../lib/StartInfo.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/texts/CGeneralTextHandler.h"

AdventureOptions::AdventureOptions()
	: CWindowObject(PLAYER_COLORED, ImagePath::builtin("ADVOPTS"))
{
	OBJECT_CONSTRUCTION;

	viewWorld = std::make_shared<CButton>(Point(24, 23), AnimationPath::builtin("ADVVIEW.DEF"), CButton::tooltip(), [&](){ close(); }, EShortcut::ADVENTURE_VIEW_WORLD);
	viewWorld->addCallback([] { GAME->interface()->viewWorldMap(); });
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "View World";
		accessInfo.description = "Show the entire world map";
		accessInfo.tabOrder = 1;
		viewWorld->setAccessibilityInfo(accessInfo);
	}

	puzzle = std::make_shared<CButton>(Point(24, 81), AnimationPath::builtin("ADVPUZ.DEF"), CButton::tooltip(), [&](){ close(); }, EShortcut::ADVENTURE_VIEW_PUZZLE);
	puzzle->addCallback(std::bind(&CPlayerInterface::showPuzzleMap, GAME->interface()));
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "View Puzzle";
		accessInfo.description = "Show the obelisk puzzle map";
		accessInfo.tabOrder = 2;
		puzzle->setAccessibilityInfo(accessInfo);
	}

	dig = std::make_shared<CButton>(Point(24, 139), AnimationPath::builtin("ADVDIG.DEF"), CButton::tooltip(), [&](){ close(); }, EShortcut::ADVENTURE_DIG_GRAIL);
	if(const CGHeroInstance *h = GAME->interface()->localState->getCurrentHero())
		dig->addCallback(std::bind(&CPlayerInterface::tryDigging, GAME->interface(), h));
	else
		dig->block(true);
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "Dig for Grail";
		accessInfo.description = "Attempt to dig for the grail at current location";
		accessInfo.tabOrder = 3;
		if (dig->isBlocked())
			accessInfo.state = "disabled";
		dig->setAccessibilityInfo(accessInfo);
	}

	scenInfo = std::make_shared<CButton>(Point(24, 198), AnimationPath::builtin("ADVINFO.DEF"), CButton::tooltip(), [&](){ close(); }, EShortcut::ADVENTURE_VIEW_SCENARIO);
	scenInfo->addCallback(AdventureOptions::showScenarioInfo);
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "Scenario Info";
		accessInfo.description = "View scenario information and objectives";
		accessInfo.tabOrder = 4;
		scenInfo->setAccessibilityInfo(accessInfo);
	}
	
	replay = std::make_shared<CButton>(Point(24, 257), AnimationPath::builtin("ADVTURN.DEF"), CButton::tooltip(), [&](){ close(); }, EShortcut::ADVENTURE_REPLAY_TURN);
	replay->addCallback([]{ GAME->interface()->showInfoDialog(LIBRARY->generaltexth->translate("vcmi.adventureMap.replayOpponentTurnNotImplemented")); });
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "Replay Turn";
		accessInfo.description = "Replay the last opponent's turn";
		accessInfo.tabOrder = 5;
		replay->setAccessibilityInfo(accessInfo);
	}

	exit = std::make_shared<CButton>(Point(203, 313), AnimationPath::builtin("IOK6432.DEF"), CButton::tooltip(), std::bind(&AdventureOptions::close, this), EShortcut::GLOBAL_RETURN);
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "OK";
		accessInfo.description = "Close adventure options";
		accessInfo.tabOrder = 6;
		exit->setAccessibilityInfo(accessInfo);
	}

	// Set window-level accessibility
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "window";
		accessInfo.name = "Adventure Options";
		accessInfo.description = "Adventure map options and actions";
		setAccessibilityInfo(accessInfo);
	}
}

void AdventureOptions::showScenarioInfo()
{
	if(GAME->interface()->cb->getStartInfo()->campState)
	{
		ENGINE->windows().createAndPushWindow<CCampaignInfoScreen>();
	}
	else
	{
		ENGINE->windows().createAndPushWindow<CScenarioInfoScreen>();
	}
}

