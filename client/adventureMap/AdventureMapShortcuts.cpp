/*
 * AdventureMapShortcuts.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "AdventureMapShortcuts.h"

#include <algorithm>

#include "../CMT.h"
#include "../CPlayerInterface.h"
#include "../CServerHandler.h"
#include "../PlayerLocalState.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/Shortcut.h"
#include "../gui/WindowHandler.h"
#include "../gui/AccessibilityManager.h"
#include "../eventsSDL/InputHandler.h"
#include "../lobby/CSavingScreen.h"
#include "../mapView/mapHandler.h"
#include "../mapView/MapViewModel.h"
#include "../windows/CKingdomInterface.h"
#include "../windows/CSpellWindow.h"
#include "../windows/CMarketWindow.h"
#include "../windows/GUIClasses.h"
#include "../windows/settings/SettingsMainWindow.h"
#include "AdventureMapInterface.h"
#include "AdventureOptions.h"
#include "AdventureState.h"
#include "AdventureMapWidget.h"
#include "../mapView/MapView.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/CPlayerState.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapObjects/CGTownInstance.h"
#include "../../lib/mapping/CMap.h"
#include "../../lib/pathfinder/CGPathNode.h"
#include "../../lib/mapObjectConstructors/CObjectClassesHandler.h"
#include "../../lib/constants/EntityIdentifiers.h"

AdventureMapShortcuts::AdventureMapShortcuts(AdventureMapInterface & owner)
	: owner(owner)
	, state(EAdventureState::NOT_INITIALIZED)
	, mapLevel(0)
	, searchLast("")
	, searchPos(0)
{}

void AdventureMapShortcuts::setState(EAdventureState newState)
{
	state = newState;
}

EAdventureState AdventureMapShortcuts::getState() const
{
	return state;
}

void AdventureMapShortcuts::onMapViewMoved(const Rect & visibleArea, int newMapLevel)
{
	mapLevel = newMapLevel;
}

std::vector<AdventureMapShortcutState> AdventureMapShortcuts::getShortcuts()
{
	std::vector<AdventureMapShortcutState> result = {
		{ EShortcut::ADVENTURE_KINGDOM_OVERVIEW, optionInMapView(),      [this]() { this->showOverview(); } },
		{ EShortcut::ADVENTURE_EXIT_WORLD_VIEW,  optionInWorldView(),    [this]() { this->worldViewBack(); } },
		{ EShortcut::ADVENTURE_VIEW_WORLD,       optionInMapView(),      [this]() { this->worldViewScale1x(); } },
		{ EShortcut::ADVENTURE_VIEW_WORLD_X1,    optionInWorldView(),    [this]() { this->worldViewScale1x(); } },
		{ EShortcut::ADVENTURE_VIEW_WORLD_X2,    optionInWorldView(),    [this]() { this->worldViewScale2x(); } },
		{ EShortcut::ADVENTURE_VIEW_WORLD_X4,    optionInWorldView(),    [this]() { this->worldViewScale4x(); } },
		{ EShortcut::ADVENTURE_TOGGLE_MAP_LEVEL, optionCanToggleLevel(), [this]() { this->switchMapLevel(); } },
		{ EShortcut::ADVENTURE_QUEST_LOG,        optionCanViewQuests(),  [this]() { this->showQuestlog(); } },
		{ EShortcut::ADVENTURE_TOGGLE_SLEEP,     optionHeroSelected(),   [this]() { this->toggleSleepWake(); } },
		{ EShortcut::ADVENTURE_TOGGLE_GRID,      optionInMapView(),      [this]() { this->toggleGrid(); } },
		{ EShortcut::ADVENTURE_TOGGLE_VISITABLE, optionInMapView(),      [this]() { this->toggleVisitable(); } },
		{ EShortcut::ADVENTURE_TOGGLE_BLOCKED,   optionInMapView(),      [this]() { this->toggleBlocked(); } },
		{ EShortcut::ADVENTURE_SCROLL_LEFT,      optionMapScrollingActive(), [this]() { this->scrollMap({-1, 0}); } },
		{ EShortcut::ADVENTURE_SCROLL_RIGHT,     optionMapScrollingActive(), [this]() { this->scrollMap({+1, 0}); } },
		{ EShortcut::ADVENTURE_SCROLL_UP,        optionMapScrollingActive(), [this]() { this->scrollMap({0, -1}); } },
		{ EShortcut::ADVENTURE_SCROLL_DOWN,      optionMapScrollingActive(), [this]() { this->scrollMap({0, +1}); } },
		{ EShortcut::ADVENTURE_TRACK_HERO,       optionInMapView(),      [this]() { this->toggleTrackHero(); } },
		{ EShortcut::ADVENTURE_SET_HERO_ASLEEP,  optionHeroAwake(),      [this]() { this->setHeroSleeping(); } },
		{ EShortcut::ADVENTURE_SET_HERO_AWAKE,   optionHeroSleeping(),   [this]() { this->setHeroAwake(); } },
		{ EShortcut::ADVENTURE_MOVE_HERO,        optionHeroCanMove(),    [this]() { this->moveHeroAlongPath(); } },
		{ EShortcut::ADVENTURE_CAST_SPELL,       optionHeroSelected(),   [this]() { this->showSpellbook(); } },
		{ EShortcut::ADVENTURE_GAME_OPTIONS,     optionInMapView(),      [this]() { this->adventureOptions(); } },
		{ EShortcut::GLOBAL_OPTIONS,             optionInMapView(),      [this]() { this->systemOptions(); } },
		{ EShortcut::ADVENTURE_FIRST_HERO,       optionInMapView(),      [this]() { this->firstHero(); } },
		{ EShortcut::ADVENTURE_NEXT_HERO,        optionHasNextHero(),    [this]() { this->nextHero(); } },
		{ EShortcut::ADVENTURE_END_TURN,         optionCanEndTurn(),     [this]() { this->endTurn(); } },
		{ EShortcut::ADVENTURE_THIEVES_GUILD,    optionInMapView(),      [this]() { this->showThievesGuild(); } },
		{ EShortcut::ADVENTURE_VIEW_SCENARIO,    optionInMapView(),      [this]() { this->showScenarioInfo(); } },
		{ EShortcut::ADVENTURE_QUIT_GAME,        optionInMapView(),      [this]() { this->quitGame(); } },
		{ EShortcut::ADVENTURE_TO_MAIN_MENU,     optionInMapView(),      [this]() { this->toMainMenu(); } },
		{ EShortcut::ADVENTURE_SAVE_GAME,        optionInMapView(),      [this]() { this->saveGame(); } },
		{ EShortcut::ADVENTURE_NEW_GAME,         optionInMapView(),      [this]() { this->newGame(); } },
		{ EShortcut::ADVENTURE_LOAD_GAME,        optionInMapView(),      [this]() { this->loadGame(); } },
		{ EShortcut::ADVENTURE_RESTART_GAME,     optionInMapView(),      [this]() { this->restartGame(); } },
		{ EShortcut::ADVENTURE_DIG_GRAIL,        optionHeroSelected(),   [this]() { this->digGrail(); } },
		{ EShortcut::ADVENTURE_VIEW_PUZZLE,      optionSidePanelActive(),[this]() { this->viewPuzzleMap(); } },
		{ EShortcut::ADVENTURE_VISIT_OBJECT,     optionCanVisitObject(), [this]() { this->visitObject(); } },
		{ EShortcut::ADVENTURE_VIEW_SELECTED,    optionInMapView(),      [this]() { this->openObject(); } },
		{ EShortcut::ADVENTURE_MARKETPLACE,      optionInMapView(),      [this]() { this->showMarketplace(); } },
		{ EShortcut::ADVENTURE_ZOOM_IN,          optionSidePanelActive(),[this]() { this->zoom(+10); } },
		{ EShortcut::ADVENTURE_ZOOM_OUT,         optionSidePanelActive(),[this]() { this->zoom(-10); } },
		{ EShortcut::ADVENTURE_ZOOM_RESET,       optionSidePanelActive(),[this]() { this->zoom( 0); } },
		{ EShortcut::ADVENTURE_FIRST_TOWN,       optionInMapView(),      [this]() { this->firstTown(); } },
		{ EShortcut::ADVENTURE_NEXT_TOWN,        optionInMapView(),      [this]() { this->nextTown(); } },
		{ EShortcut::ADVENTURE_NEXT_OBJECT,      optionInMapView(),      [this]() { this->nextObject(); } },
		{ EShortcut::ADVENTURE_MOVE_HERO_SW,     optionHeroSelected(),   [this]() { this->moveHeroDirectional({-1, +1}); } },
		{ EShortcut::ADVENTURE_MOVE_HERO_SS,     optionHeroSelected(),   [this]() { this->moveHeroDirectional({ 0, +1}); } },
		{ EShortcut::ADVENTURE_MOVE_HERO_SE,     optionHeroSelected(),   [this]() { this->moveHeroDirectional({+1, +1}); } },
		{ EShortcut::ADVENTURE_MOVE_HERO_WW,     optionHeroSelected(),   [this]() { this->moveHeroDirectional({-1,  0}); } },
		{ EShortcut::ADVENTURE_MOVE_HERO_EE,     optionHeroSelected(),   [this]() { this->moveHeroDirectional({+1,  0}); } },
		{ EShortcut::ADVENTURE_MOVE_HERO_NW,     optionHeroSelected(),   [this]() { this->moveHeroDirectional({-1, -1}); } },
		{ EShortcut::ADVENTURE_MOVE_HERO_NN,     optionHeroSelected(),   [this]() { this->moveHeroDirectional({ 0, -1}); } },
		{ EShortcut::ADVENTURE_MOVE_HERO_NE,     optionHeroSelected(),   [this]() { this->moveHeroDirectional({+1, -1}); } },
		{ EShortcut::ADVENTURE_SEARCH,           optionSidePanelActive(),[this]() { this->search(false); } },
		{ EShortcut::ADVENTURE_SEARCH_CONTINUE,  optionSidePanelActive(),[this]() { this->search(true); } },
		{ EShortcut::ADVENTURE_ANNOUNCE_LANDMARKS, optionHeroSelected(),  [this]() { this->announceLandmarks(); } }
	};
	return result;
}

void AdventureMapShortcuts::showOverview()
{
	ENGINE->windows().createAndPushWindow<CKingdomInterface>();
}

void AdventureMapShortcuts::worldViewBack()
{
	owner.hotkeyExitWorldView();

	auto hero = GAME->interface()->localState->getCurrentHero();
	if (hero)
		owner.centerOnObject(hero);
}

void AdventureMapShortcuts::worldViewScale1x()
{
	// TODO set corresponding scale button to "selected" mode
	owner.openWorldView(7);
}

void AdventureMapShortcuts::worldViewScale2x()
{
	owner.openWorldView(11);
}

void AdventureMapShortcuts::worldViewScale4x()
{
	owner.openWorldView(16);
}

void AdventureMapShortcuts::switchMapLevel()
{
	int maxLevels = GAME->interface()->cb->getMapSize().z;
	if (maxLevels < 2)
		return;

	owner.hotkeySwitchMapLevel();
}

void AdventureMapShortcuts::showQuestlog()
{
	GAME->interface()->showQuestLog();
}

void AdventureMapShortcuts::toggleTrackHero()
{
	Settings s = settings.write["session"];
	s["adventureTrackHero"].Bool() = !settings["session"]["adventureTrackHero"].Bool();
}

void AdventureMapShortcuts::toggleGrid()
{
	Settings s = settings.write["gameTweaks"];
	s["showGrid"].Bool() = !settings["gameTweaks"]["showGrid"].Bool();
}

void AdventureMapShortcuts::toggleVisitable()
{
	Settings s = settings.write["session"];
	s["showVisitable"].Bool() = !settings["session"]["showVisitable"].Bool();
}

void AdventureMapShortcuts::toggleBlocked()
{
	Settings s = settings.write["session"];
	s["showBlocked"].Bool() = !settings["session"]["showBlocked"].Bool();
}

void AdventureMapShortcuts::toggleSleepWake()
{
	if (!optionHeroSelected())
		return;

	if (optionHeroAwake())
		setHeroSleeping();
	else
		setHeroAwake();
}

void AdventureMapShortcuts::setHeroSleeping()
{
	const CGHeroInstance *h = GAME->interface()->localState->getCurrentHero();
	if (h)
	{
		GAME->interface()->localState->setHeroAsleep(h);
		owner.onHeroChanged(h);
		nextHero();
	}
}

void AdventureMapShortcuts::setHeroAwake()
{
	const CGHeroInstance *h = GAME->interface()->localState->getCurrentHero();
	if (h)
	{
		GAME->interface()->localState->setHeroAwaken(h);
		owner.onHeroChanged(h);
	}
}

void AdventureMapShortcuts::moveHeroAlongPath()
{
	const CGHeroInstance *h = GAME->interface()->localState->getCurrentHero();
	if (!h || !GAME->interface()->localState->hasPath(h))
		return;

	GAME->interface()->moveHero(h, GAME->interface()->localState->getPath(h));
}

void AdventureMapShortcuts::showSpellbook()
{
	if (!GAME->interface()->localState->getCurrentHero())
		return;

	owner.centerOnObject(GAME->interface()->localState->getCurrentHero());

	ENGINE->windows().createAndPushWindow<CSpellWindow>(GAME->interface()->localState->getCurrentHero(), GAME->interface(), false);
}

void AdventureMapShortcuts::adventureOptions()
{
	ENGINE->windows().createAndPushWindow<AdventureOptions>();
}

void AdventureMapShortcuts::systemOptions()
{
	ENGINE->windows().createAndPushWindow<SettingsMainWindow>();
}

void AdventureMapShortcuts::firstHero()
{
	if (!GAME->interface()->localState->getWanderingHeroes().empty())
	{
		const auto * hero = GAME->interface()->localState->getWanderingHero(0);
		GAME->interface()->localState->setSelection(hero);
		owner.centerOnObject(hero);
	}
}

void AdventureMapShortcuts::nextHero()
{
	const auto * currHero = GAME->interface()->localState->getCurrentHero();
	const auto * nextHero = GAME->interface()->localState->getNextWanderingHero(currHero);

	if (nextHero)
	{
		GAME->interface()->localState->setSelection(nextHero);
		owner.centerOnObject(nextHero);
	}
}

void AdventureMapShortcuts::endTurn()
{
	if(!GAME->interface()->makingTurn)
		return;

	if(settings["adventure"]["heroReminder"].Bool())
	{
		for(auto hero : GAME->interface()->localState->getWanderingHeroes())
		{
			if(!GAME->interface()->localState->isHeroSleeping(hero) && hero->movementPointsRemaining() > 0)
			{
				// Only show hero reminder if conditions met:
				// - There still movement points
				// - Hero don't have a path or there not points for first step on path
				GAME->interface()->localState->verifyPath(hero);

				if(!GAME->interface()->localState->hasPath(hero))
				{
					GAME->interface()->showYesNoDialog( LIBRARY->generaltexth->allTexts[55], [this](){ owner.hotkeyEndingTurn(); }, nullptr);
					return;
				}

				auto path = GAME->interface()->localState->getPath(hero);
				if (path.nodes.size() < 2 || path.nodes[path.nodes.size() - 2].turns)
				{
					GAME->interface()->showYesNoDialog( LIBRARY->generaltexth->allTexts[55], [this](){ owner.hotkeyEndingTurn(); }, nullptr);
					return;
				}
			}
		}
	}
	owner.hotkeyEndingTurn();
}

void AdventureMapShortcuts::showThievesGuild()
{
	//find first town with tavern
	auto itr = boost::range::find_if(GAME->interface()->localState->getOwnedTowns(), [](const CGTownInstance * town)
	{
		return town->hasBuilt(BuildingID::TAVERN);
	});

	if(itr != GAME->interface()->localState->getOwnedTowns().end())
		GAME->interface()->showThievesGuildWindow(*itr);
	else
		GAME->interface()->showInfoDialog(LIBRARY->generaltexth->translate("vcmi.adventureMap.noTownWithTavern"));
}

void AdventureMapShortcuts::showScenarioInfo()
{
	AdventureOptions::showScenarioInfo();
}

void AdventureMapShortcuts::toMainMenu()
{
	GAME->interface()->showYesNoDialog(
		LIBRARY->generaltexth->allTexts[578],
		[]()
		{
			GAME->server().endGameplay();
			GAME->mainmenu()->menu->switchToTab("main");
		},
		0
		);
}

void AdventureMapShortcuts::newGame()
{
	GAME->interface()->showYesNoDialog(
		LIBRARY->generaltexth->allTexts[578],
		[]()
		{
			GAME->server().endGameplay();
			GAME->mainmenu()->menu->switchToTab("new");
		},
		nullptr
		);
}

void AdventureMapShortcuts::quitGame()
{
	GAME->interface()->showYesNoDialog(
		LIBRARY->generaltexth->allTexts[578],
		[](){ GAME->onShutdownRequested(false);},
		nullptr
		);
}

void AdventureMapShortcuts::saveGame()
{
	ENGINE->windows().createAndPushWindow<CSavingScreen>();
}

void AdventureMapShortcuts::loadGame()
{
	GAME->interface()->proposeLoadingGame();
}

void AdventureMapShortcuts::digGrail()
{
	const CGHeroInstance *h = GAME->interface()->localState->getCurrentHero();

	if(h && GAME->interface()->makingTurn)
		GAME->interface()->tryDigging(h);
}

void AdventureMapShortcuts::viewPuzzleMap()
{
	GAME->interface()->showPuzzleMap();
}

void AdventureMapShortcuts::restartGame()
{
	GAME->interface()->showYesNoDialog(
		LIBRARY->generaltexth->translate("vcmi.adventureMap.confirmRestartGame"),
		[]()
		{
			ENGINE->dispatchMainThread(
				[]()
				{
					GAME->server().sendRestartGame();
				}
			);
		},
		nullptr
	);
}

void AdventureMapShortcuts::visitObject()
{
	const CGHeroInstance *h = GAME->interface()->localState->getCurrentHero();

	if(h)
		GAME->interface()->cb->moveHero(h, h->pos, false);
}

void AdventureMapShortcuts::openObject()
{
	const CGHeroInstance *h = GAME->interface()->localState->getCurrentHero();
	const CGTownInstance *t = GAME->interface()->localState->getCurrentTown();
	if(h)
		GAME->interface()->openHeroWindow(h);

	if(t)
		GAME->interface()->openTownWindow(t);
}

void AdventureMapShortcuts::showMarketplace()
{
	//check if we have any marketplace
	const CGTownInstance *townWithMarket = nullptr;
	for(const CGTownInstance *t : GAME->interface()->cb->getTownsInfo())
	{
		if(t->hasBuilt(BuildingID::MARKETPLACE))
		{
			townWithMarket = t;
			break;
		}
	}

	if(townWithMarket) //if any town has marketplace, open window
		ENGINE->windows().createAndPushWindow<CMarketWindow>(townWithMarket, nullptr, nullptr, EMarketMode::RESOURCE_RESOURCE);
	else //if not - complain
		GAME->interface()->showInfoDialog(LIBRARY->generaltexth->translate("vcmi.adventureMap.noTownWithMarket"));
}

void AdventureMapShortcuts::firstTown()
{
	if (!GAME->interface()->localState->getOwnedTowns().empty())
	{
		const auto * town = GAME->interface()->localState->getOwnedTown(0);
		GAME->interface()->localState->setSelection(town);
		owner.centerOnObject(town);
	}
}

void AdventureMapShortcuts::nextTown()
{
	owner.hotkeyNextTown();
}

void AdventureMapShortcuts::zoom( int distance)
{
	owner.hotkeyZoom(distance, false);
}

void AdventureMapShortcuts::search(bool next)
{
	// get all relevant objects
	std::vector<ObjectInstanceID> visitableObjInstances;
	for(auto & obj : GAME->interface()->cb->getAllVisitableObjs())
		if(obj->ID != MapObjectID::MONSTER && obj->ID != MapObjectID::HERO && obj->ID != MapObjectID::TOWN)
			visitableObjInstances.push_back(obj->id);

	// count of elements for each group (map is already sorted)
	std::map<std::string, int> mapObjCount;
	for(auto & obj : visitableObjInstances)
		mapObjCount[{ GAME->interface()->cb->getObjInstance(obj)->getObjectName() }]++;

	// convert to vector for indexed access
	std::vector<std::pair<std::string, int>> textCountList;
	for (auto itr = mapObjCount.begin(); itr != mapObjCount.end(); ++itr)
		textCountList.push_back(*itr);

	// get pos of last selection
	int lastSel = 0;
	for(int i = 0; i < textCountList.size(); i++)
		if(textCountList[i].first == searchLast)
			lastSel = i;

	// create texts
	std::vector<std::string> texts;
	for(auto & obj : textCountList)
		texts.push_back(obj.first + " (" + std::to_string(obj.second) + ")");

	// function to center element from list on map
	auto selectObjOnMap = [this, textCountList, visitableObjInstances](int index)
		{
			auto selObj = textCountList[index].first;

			// filter for matching objects
			std::vector<ObjectInstanceID> selVisitableObjInstances;
			for(auto & obj : visitableObjInstances)
				if(selObj == GAME->interface()->cb->getObjInstance(obj)->getObjectName())
					selVisitableObjInstances.push_back(obj);
			
			if(searchPos + 1 < selVisitableObjInstances.size() && searchLast == selObj)
				searchPos++;
			else
				searchPos = 0;

			auto objInst = GAME->interface()->cb->getObjInstance(selVisitableObjInstances[searchPos]);
			owner.centerOnObject(objInst);
			searchLast = objInst->getObjectName();
		};

	if(next)
		selectObjOnMap(lastSel);
	else
		ENGINE->windows().createAndPushWindow<CObjectListWindow>(texts, nullptr, LIBRARY->generaltexth->translate("vcmi.adventureMap.search.hover"), LIBRARY->generaltexth->translate("vcmi.adventureMap.search.help"), [selectObjOnMap](int index){ selectObjOnMap(index); }, lastSel, std::vector<std::shared_ptr<IImage>>(), true);
}

void AdventureMapShortcuts::nextObject()
{
	const CGHeroInstance *h = GAME->interface()->localState->getCurrentHero();
	const CGTownInstance *t = GAME->interface()->localState->getCurrentTown();
	if(h)
		nextHero();

	if(t)
		nextTown();
}

void AdventureMapShortcuts::moveHeroDirectional(const Point & direction)
{
	// Suppress hero selection announcements for the entire duration of directional movement
	adventureInt->setSuppressHeroSelectionAnnouncement(true);
	
	const CGHeroInstance *h = GAME->interface()->localState->getCurrentHero(); //selected hero

	if(!h)
	{
		adventureInt->setSuppressHeroSelectionAnnouncement(false);
		return;
	}

	if (GAME->map().hasOngoingAnimations())
	{
		adventureInt->setSuppressHeroSelectionAnnouncement(false);
		return;
	}

	int3 dst = h->visitablePos() + int3(direction.x, direction.y, 0);

	if (!GAME->map().isInMap((dst)))
	{
		adventureInt->setSuppressHeroSelectionAnnouncement(false);
		return;
	}

	if ( !GAME->interface()->localState->setPath(h, dst))
	{
		adventureInt->setSuppressHeroSelectionAnnouncement(false);
		return;
	}

	const CGPath & path = GAME->interface()->localState->getPath(h);

	// For directional movement, we want to move only one tile at a time
	// Check if we can move (no turns required for next node)
	if (path.nodes.size() >= 2 && path.nextNode().turns == 0)
	{
		// Create a simple two-node path for single tile movement
		CGPath singleStepPath;
		// Path nodes are stored in reverse order (destination first, current position last)
		singleStepPath.nodes.push_back(path.nodes[path.nodes.size()-2]); // next position (destination)
		singleStepPath.nodes.push_back(path.nodes[path.nodes.size()-1]); // current position
		
		// Clear any existing path to prevent continuous movement after this single step
		GAME->interface()->localState->erasePath(h);
		
		// Move hero one tile - HeroMovementController will detect this as directional movement
		GAME->interface()->moveHero(h, singleStepPath);
		
		// The suppression will be cleared after movement completes
		// The reordering in endMove ensures no unwanted announcements
	}
	else
	{
		// Movement not possible, clear suppression
		adventureInt->setSuppressHeroSelectionAnnouncement(false);
	}
}

void AdventureMapShortcuts::scrollMap(const Point & direction)
{
	// Calculate scroll distance based on settings
	int32_t scrollSpeedPixels = settings["adventure"]["scrollSpeedPixels"].Float();
	// Use a fixed scroll distance for keyboard scrolling (roughly 100ms worth of scrolling)
	int32_t scrollDistance = scrollSpeedPixels / 10;
	
	Point scrollDelta = direction * scrollDistance;
	
	// Scroll the map
	owner.scrollMap(scrollDelta);
	
	// Note: We cannot access the map view's model directly as it's not exposed through the public interface.
	// The position announcement functionality has been removed to fix compilation errors.
	// If position announcements are needed, the AdventureMapInterface would need to expose
	// a public method to get the current map center or visible area.
}

bool AdventureMapShortcuts::optionCanViewQuests()
{
	return optionInMapView() && !GAME->interface()->cb->getPlayerState(GAME->interface()->playerID)->quests.empty();
}

bool AdventureMapShortcuts::optionCanToggleLevel()
{
	return optionSidePanelActive() && GAME->interface()->cb->getMapSize().z > 1;
}

bool AdventureMapShortcuts::optionMapLevelSurface()
{
	return mapLevel == 0;
}

bool AdventureMapShortcuts::optionHeroSleeping()
{
	const CGHeroInstance *hero = GAME->interface()->localState->getCurrentHero();
	return optionInMapView() && hero && GAME->interface()->localState->isHeroSleeping(hero);
}

bool AdventureMapShortcuts::optionHeroAwake()
{
	const CGHeroInstance *hero = GAME->interface()->localState->getCurrentHero();
	return optionInMapView() && hero && !GAME->interface()->localState->isHeroSleeping(hero);
}

bool AdventureMapShortcuts::optionCanVisitObject()
{
	if (!optionHeroSelected())
		return false;

	auto * hero = GAME->interface()->localState->getCurrentHero();
	auto objects = GAME->interface()->cb->getVisitableObjs(hero->visitablePos());

	return objects.size() > 1; // there is object other than our hero
}

bool AdventureMapShortcuts::optionHeroSelected()
{
	// Don't allow hero movement shortcuts when Ctrl is pressed (cursor mode)
	if (ENGINE->input().isKeyboardCtrlDown())
		return false;
		
	return optionInMapView() && GAME->interface()->localState->getCurrentHero() != nullptr;
}

bool AdventureMapShortcuts::optionHeroCanMove()
{
	const auto * hero = GAME->interface()->localState->getCurrentHero();
	return optionInMapView() && hero && GAME->interface()->localState->hasPath(hero) && GAME->interface()->localState->getPath(hero).nextNode().turns == 0;
}

bool AdventureMapShortcuts::optionHasNextHero()
{
	const auto * hero = GAME->interface()->localState->getCurrentHero();
	const auto * nextSuitableHero = GAME->interface()->localState->getNextWanderingHero(hero);

	return optionInMapView() && nextSuitableHero != nullptr;
}

bool AdventureMapShortcuts::optionCanEndTurn()
{
	return optionInMapView() && GAME->interface()->makingTurn;
}

bool AdventureMapShortcuts::optionSpellcasting()
{
	return state == EAdventureState::CASTING_SPELL;
}

bool AdventureMapShortcuts::optionInMapView()
{
	return state == EAdventureState::MAKING_TURN;
}

bool AdventureMapShortcuts::optionInWorldView()
{
	return state == EAdventureState::WORLD_VIEW;
}

bool AdventureMapShortcuts::optionSidePanelActive()
{
return state == EAdventureState::MAKING_TURN || state == EAdventureState::WORLD_VIEW;
}

bool AdventureMapShortcuts::optionMapScrollingActive()
{
	return state == EAdventureState::MAKING_TURN || state == EAdventureState::WORLD_VIEW;
}

bool AdventureMapShortcuts::optionMapViewActive()
{
	return state == EAdventureState::MAKING_TURN || state == EAdventureState::WORLD_VIEW || state == EAdventureState::CASTING_SPELL;
}

void AdventureMapShortcuts::announceLandmarks()
{
	const CGHeroInstance *hero = GAME->interface()->localState->getCurrentHero();
	if (!hero)
		return;

	// Get hero's current position
	int3 heroPos = hero->visitablePos();
	
	const int SCAN_RADIUS = hero->getSightRadius();
	
	// Categories of objects and their priorities (lower number = higher priority)
	enum ObjectPriority {
		THREAT = 1,
		IMPORTANT_STRUCTURE = 2,
		VALUABLE = 3,
		RESOURCE = 4,
		OTHER = 5
	};
	
	struct LandmarkInfo {
		const CGObjectInstance* obj;
		int distance;
		int priority;
		std::string direction;
		
		bool operator<(const LandmarkInfo& other) const {
			// Sort by priority first, then by distance
			if (priority != other.priority)
				return priority < other.priority;
			return distance < other.distance;
		}
	};
	
	std::vector<LandmarkInfo> landmarks;
	
	// Scan for objects within radius
	for (int dx = -SCAN_RADIUS; dx <= SCAN_RADIUS; dx++)
	{
		for (int dy = -SCAN_RADIUS; dy <= SCAN_RADIUS; dy++)
		{
			int3 checkPos = heroPos + int3(dx, dy, heroPos.z);
			
			if (!GAME->interface()->cb->isInTheMap(checkPos))
				continue;
				
			if (checkPos == heroPos)
				continue;

			if (!GAME->interface()->cb->isVisible(checkPos))
				continue;

			auto objects = GAME->interface()->cb->getVisitableObjs(checkPos);
			
			for (const auto* obj : objects)
			{
				if (!obj || obj == hero)
					continue;

				int distance = std::max(std::abs(dx), std::abs(dy)); // Chebyshev distance
				std::string direction;
				if (dy < 0 && dx == 0) direction = "north";
				else if (dy > 0 && dx == 0) direction = "south";
				else if (dx < 0 && dy == 0) direction = "west";
				else if (dx > 0 && dy == 0) direction = "east";
				else if (dy < 0 && dx < 0) direction = "northwest";
				else if (dy < 0 && dx > 0) direction = "northeast";
				else if (dy > 0 && dx < 0) direction = "southwest";
				else if (dy > 0 && dx > 0) direction = "southeast";
	
				int priority = OTHER;

				if (obj->ID == MapObjectID::HERO)
				{
					const auto* heroObj = dynamic_cast<const CGHeroInstance*>(obj);
					if (heroObj && heroObj->tempOwner != hero->tempOwner)
					{
						priority = THREAT; // Enemy heroes are threats
						landmarks.push_back({obj, distance, priority, direction});
						continue;
					}
				}
				
				switch(obj->ID)
				{
					case MapObjectID::MONSTER:
					case MapObjectID::DRAGON_UTOPIA:
					case MapObjectID::PANDORAS_BOX:
					case MapObjectID::CREATURE_BANK:
					case MapObjectID::CREATURE_GENERATOR1:
					case MapObjectID::CREATURE_GENERATOR2:
					case MapObjectID::CREATURE_GENERATOR3:
					case MapObjectID::CREATURE_GENERATOR4:
						priority = THREAT;
						break;
						
					case MapObjectID::TOWN:
					case MapObjectID::GARRISON:
					case MapObjectID::GARRISON2:
					case MapObjectID::MONOLITH_ONE_WAY_ENTRANCE:
					case MapObjectID::MONOLITH_ONE_WAY_EXIT:
					case MapObjectID::MONOLITH_TWO_WAY:
					case MapObjectID::SUBTERRANEAN_GATE:
					case MapObjectID::WHIRLPOOL:
					case MapObjectID::SHIPYARD:
					case MapObjectID::LIGHTHOUSE:
					case MapObjectID::BORDERGUARD:
					case MapObjectID::BORDER_GATE:
					case MapObjectID::TRADING_POST:
					case MapObjectID::TRADING_POST_SNOW:
						priority = IMPORTANT_STRUCTURE;
						break;
						
					case MapObjectID::TREASURE_CHEST:
					case MapObjectID::SPELL_SCROLL:
					case MapObjectID::ARTIFACT:
					case MapObjectID::CAMPFIRE:
					case MapObjectID::SHRINE_OF_MAGIC_INCANTATION:
					case MapObjectID::SHRINE_OF_MAGIC_GESTURE:
					case MapObjectID::SHRINE_OF_MAGIC_THOUGHT:
					case MapObjectID::SEER_HUT:
					case MapObjectID::QUEST_GUARD:
					case MapObjectID::GRAIL:
					case MapObjectID::PRISON:
					case MapObjectID::OBELISK:
					case MapObjectID::BLACK_MARKET:
					case MapObjectID::HILL_FORT:
					case MapObjectID::TREE_OF_KNOWLEDGE:
					case MapObjectID::LIBRARY_OF_ENLIGHTENMENT:
					case MapObjectID::WITCH_HUT:
					case MapObjectID::SCHOOL_OF_MAGIC:
					case MapObjectID::SCHOOL_OF_WAR:
					case MapObjectID::UNIVERSITY:
					case MapObjectID::MERCENARY_CAMP:
					case MapObjectID::ALTAR_OF_SACRIFICE:
					case MapObjectID::ARENA:
					case MapObjectID::STABLES:
					case MapObjectID::WAR_MACHINE_FACTORY:
					case MapObjectID::FREELANCERS_GUILD:
					case MapObjectID::TEMPLE:
					case MapObjectID::DEN_OF_THIEVES:
					case MapObjectID::CARTOGRAPHER:
					case MapObjectID::LEARNING_STONE:
					case MapObjectID::GARDEN_OF_REVELATION:
					case MapObjectID::TAVERN:
					case MapObjectID::REFUGEE_CAMP:
					case MapObjectID::KEYMASTER:
						priority = VALUABLE;
						break;
						
					case MapObjectID::RESOURCE:
					case MapObjectID::MINE:
					case MapObjectID::ABANDONED_MINE:
					case MapObjectID::WINDMILL:
					case MapObjectID::WATER_WHEEL:
					case MapObjectID::MYSTICAL_GARDEN:
					case MapObjectID::BOAT:
						priority = RESOURCE;
						break;
				}
				
				landmarks.push_back({obj, distance, priority, direction});
			}
		}
	}
	
	std::sort(landmarks.begin(), landmarks.end());
	
	std::string announcement;
	
	if (landmarks.empty())
	{
		announcement = "No landmarks detected within scanning range.";
	}
	else
	{
		announcement = "";
		int count = 0;
		for (const auto& landmark : landmarks)
		{
			if (count > 0)
				announcement += "; ";

			announcement += landmark.obj->getObjectName();
			announcement += " " + std::to_string(landmark.distance) + " tiles " + landmark.direction;

			if (landmark.obj->ID == MapObjectID::MONSTER)
			{
				announcement += " (threat)";
			}
			else if (landmark.obj->ID == MapObjectID::TOWN)
			{
				const CGTownInstance* town = dynamic_cast<const CGTownInstance*>(landmark.obj);
				if (town)
				{
					if (town->tempOwner == hero->tempOwner)
						announcement += " (friendly)";
					else if (town->tempOwner == PlayerColor::NEUTRAL)
						announcement += " (neutral)";
					else
						announcement += " (enemy)";
				}
			}
			
			count++;
		}
	}
	AccessibilityManager::getInstance().announce(announcement, true);
}
