/*
 * AdventureMapShortcuts.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include "../../lib/int3.h"

VCMI_LIB_NAMESPACE_BEGIN
class Point;
class Rect;
class CGObjectInstance;
VCMI_LIB_NAMESPACE_END

enum class EShortcut;
class AdventureMapInterface;
enum class EAdventureState;

struct AdventureMapShortcutState
{
	EShortcut shortcut;
	bool isEnabled;
	std::function<void()> callback;
};

/// Class that contains list of functions for shortcuts available from adventure map
class AdventureMapShortcuts
{
	AdventureMapInterface & owner;
	EAdventureState state;
	int mapLevel;

	std::string searchLast;
	int searchPos;
	
	void showOverview();
	void worldViewBack();
	void worldViewScale1x();
	void worldViewScale2x();
	void worldViewScale4x();
	void switchMapLevel();
	void showQuestlog();
	void toggleTrackHero();
	void toggleGrid();
	void toggleVisitable();
	void toggleBlocked();
	void toggleSleepWake();
	void setHeroSleeping();
	void setHeroAwake();
	void moveHeroAlongPath();
	void showSpellbook();
	void adventureOptions();
	void systemOptions();
	void firstHero();
	void nextHero();
	void endTurn();
	void showThievesGuild();
	void showScenarioInfo();
	void toMainMenu();
	void newGame();
	void quitGame();
	void saveGame();
	void loadGame();
	void digGrail();
	void viewPuzzleMap();
	void restartGame();
	void visitObject();
	void openObject();
	void showMarketplace();
	void firstTown();
	void nextTown();
	void nextObject();
	void zoom( int distance);
	void search(bool next);
	void moveHeroDirectional(const Point & direction);
	void scrollMap(const Point & direction);
	void announceLandmarks();
	void cycleLandmarksForward();
	void cycleLandmarksBackward();
	void scanForLandmarks(); // Helper to scan landmarks without announcing

	// Landmark cycling state
	struct LandmarkInfo {
		const CGObjectInstance* obj;
		int distance;
		int priority;
		std::string direction;
		
		bool operator<(const LandmarkInfo& other) const {
			if (priority != other.priority)
				return priority < other.priority;
			return distance < other.distance;
		}
	};
	
	std::vector<LandmarkInfo> collectedLandmarks;
	int currentLandmarkIndex = -1;
	int3 lastScanPosition; // Track where we last scanned from

public:
	explicit AdventureMapShortcuts(AdventureMapInterface & owner);

	std::vector<AdventureMapShortcutState> getShortcuts();

	bool optionCanViewQuests();
	bool optionCanToggleLevel();
	bool optionMapLevelSurface();
	bool optionHeroSleeping();
	bool optionHeroAwake();
	bool optionHeroSelected();
	bool optionHeroCanMove();
	bool optionHasNextHero();
	bool optionCanVisitObject();
	bool optionCanEndTurn();
	bool optionSpellcasting();
	bool optionInMapView();
	bool optionInWorldView();
	bool optionSidePanelActive();
	bool optionMapScrollingActive();
	bool optionMapViewActive();

	void setState(EAdventureState newState);
	EAdventureState getState() const;
	void onMapViewMoved(const Rect & visibleArea, int mapLevel);
};
