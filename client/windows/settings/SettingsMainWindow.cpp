/*
 * SettingsMainContainer.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "SettingsMainWindow.h"

#include "AdventureOptionsTab.h"
#include "BattleOptionsTab.h"
#include "GeneralOptionsTab.h"
#include "OtherOptionsTab.h"

#include "CMT.h"
#include "../../../lib/texts/CGeneralTextHandler.h"
#include "../../../lib/GameLibrary.h"
#include "CPlayerInterface.h"
#include "CServerHandler.h"
#include "../../../lib/filesystem/ResourcePath.h"
#include "../../GameEngine.h"
#include "../../GameInstance.h"
#include "gui/WindowHandler.h"
#include "gui/AccessibilityManager.h"
#include "render/Canvas.h"
#include "lobby/CSavingScreen.h"
#include "widgets/Buttons.h"
#include "widgets/Images.h"
#include "widgets/ObjectLists.h"
#include "windows/CMessage.h"

SettingsMainWindow::SettingsMainWindow(BattleInterface * parentBattleUi) : InterfaceObjectConfigurable()
{
	OBJECT_CONSTRUCTION;

	const JsonNode config(JsonPath::builtin("config/widgets/settings/settingsMainContainer.json"));
	addCallback("activateSettingsTab", [this](int tabId) { openTab(tabId); });
	addCallback("loadGame", [this](int) { loadGameButtonCallback(); });
	addCallback("saveGame", [this](int) { saveGameButtonCallback(); });
	addCallback("restartGame", [this](int) { restartGameButtonCallback(); });
	addCallback("quitGame", [this](int) { quitGameButtonCallback(); });
	addCallback("returnToMainMenu", [this](int) { mainMenuButtonCallback(); });
	addCallback("closeWindow", [this](int) { backButtonCallback(); });
	build(config);

	addUsedEvents(INPUT_MODE_CHANGE);

	std::shared_ptr<CIntObject> background = widget<CIntObject>("background");
	pos.w = background->pos.w;
	pos.h = background->pos.h;
	pos = center();

	std::shared_ptr<CButton> loadButton = widget<CButton>("loadButton");
	assert(loadButton);

	std::shared_ptr<CButton> saveButton = widget<CButton>("saveButton");
	assert(saveButton);

	std::shared_ptr<CButton> restartButton = widget<CButton>("restartButton");
	assert(restartButton);

	loadButton->block(GAME->server().isGuest());
	saveButton->block(GAME->server().isGuest() || parentBattleUi);
	restartButton->block(GAME->server().isGuest());

	int defaultTabIndex = 0;
	if(parentBattleUi != nullptr)
		defaultTabIndex = 2;
	else if(settings["general"]["lastSettingsTab"].isNumber())
		defaultTabIndex = settings["general"]["lastSettingsTab"].Integer();

	parentBattleInterface = parentBattleUi;
	tabContentArea = std::make_shared<CTabbedInt>(std::bind(&SettingsMainWindow::createTab, this, _1), Point(0, 0), defaultTabIndex);
	tabContentArea->setRedrawParent(true);

	std::shared_ptr<CToggleGroup> mainTabs = widget<CToggleGroup>("settingsTabs");
	mainTabs->setSelected(defaultTabIndex);
	// Add accessibility to settings tabs
	for (auto& [index, button] : mainTabs->buttons)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "tab";
		switch(index)
		{
			case 0: accessInfo.name = "General"; accessInfo.description = "General game settings"; break;
			case 1: accessInfo.name = "Adventure"; accessInfo.description = "Adventure map settings"; break;
			case 2: accessInfo.name = "Battle"; accessInfo.description = "Battle settings"; break;
			case 3: accessInfo.name = "Other"; accessInfo.description = "Other game settings"; break;
			default: accessInfo.name = "Settings tab"; break;
		}
		accessInfo.tabOrder = index + 1; // Tab order 1-4
		if (auto toggleButton = std::dynamic_pointer_cast<CToggleButton>(button))
			toggleButton->setAccessibilityInfo(accessInfo);
	}

	// Add accessibility to action buttons
	if (loadButton)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "Load Game";
		accessInfo.description = "Load a previously saved game";
		accessInfo.tabOrder = 10;
		if (loadButton->isBlocked())
			accessInfo.state = "disabled";
		loadButton->setAccessibilityInfo(accessInfo);
	}
	
	if (saveButton)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "Save Game";
		accessInfo.description = "Save the current game";
		accessInfo.tabOrder = 11;
		if (saveButton->isBlocked())
			accessInfo.state = "disabled";
		saveButton->setAccessibilityInfo(accessInfo);
	}
	
	if (restartButton)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "Restart Game";
		accessInfo.description = "Restart the current game from the beginning";
		accessInfo.tabOrder = 12;
		if (restartButton->isBlocked())
			accessInfo.state = "disabled";
		restartButton->setAccessibilityInfo(accessInfo);
	}
	
	std::shared_ptr<CButton> quitButton = widget<CButton>("quitButton");
	if (quitButton)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "Quit Game";
		accessInfo.description = "Exit VCMI completely";
		accessInfo.tabOrder = 13;
		quitButton->setAccessibilityInfo(accessInfo);
	}
	
	std::shared_ptr<CButton> mainMenuButton = widget<CButton>("mainMenuButton");
	if (mainMenuButton)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "Main Menu";
		accessInfo.description = "Return to the main menu";
		accessInfo.tabOrder = 14;
		mainMenuButton->setAccessibilityInfo(accessInfo);
	}

	// Add accessibility to window buttons
	std::shared_ptr<CButton> backButton = widget<CButton>("closeWindow");
	if (backButton)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "button";
		accessInfo.name = "Close";
		accessInfo.description = "Close settings window";
		accessInfo.tabOrder = 100; // At the end
		backButton->setAccessibilityInfo(accessInfo);
	}

	// Set window-level accessibility
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "window";
		accessInfo.name = "Game Settings";
		accessInfo.description = "Configure game options and preferences";
		setAccessibilityInfo(accessInfo);
	}
	
	// Announce window opening
	if (AccessibilityManager::getInstance().isScreenReaderEnabled())
	{
		AccessibilityManager::getInstance().announce("Game Settings window opened. Use tabs to navigate between different settings categories.");
	}
	
	GAME->interface()->gamePause(true);
}

std::shared_ptr<CIntObject> SettingsMainWindow::createTab(size_t index)
{
	switch(index)
	{
		case 0:
			return std::make_shared<GeneralOptionsTab>();
		case 1:
			return std::make_shared<AdventureOptionsTab>();
		case 2:
			return std::make_shared<BattleOptionsTab>(parentBattleInterface);
		case 3:
			return std::make_shared<OtherOptionsTab>();
		default:
			logGlobal->error("Wrong settings tab ID!");
			return std::make_shared<GeneralOptionsTab>();
	}
}

void SettingsMainWindow::openTab(size_t index)
{
	tabContentArea->setActive(index);
	CIntObject::redraw();

	Settings lastUsedTab = settings.write["general"]["lastSettingsTab"];
	lastUsedTab->Integer() = index;
}

void SettingsMainWindow::close()
{
	if(!ENGINE->windows().isTopWindow(this))
		logGlobal->error("Only top interface must be closed");
	
	GAME->interface()->gamePause(false);
	ENGINE->windows().popWindows(1);
}

void SettingsMainWindow::quitGameButtonCallback()
{
	GAME->interface()->showYesNoDialog(
		LIBRARY->generaltexth->allTexts[578],
		[this]()
		{
			close();
			ENGINE->user().onShutdownRequested(false);
		},
		nullptr
	);
}

void SettingsMainWindow::backButtonCallback()
{
	close();
}

void SettingsMainWindow::mainMenuButtonCallback()
{
	GAME->interface()->showYesNoDialog(
		LIBRARY->generaltexth->allTexts[578],
		[this]()
		{
			close();
			GAME->server().endGameplay();
			GAME->mainmenu()->menu->switchToTab("main");
		},
		0
	);
}

void SettingsMainWindow::loadGameButtonCallback()
{
	close();
	GAME->interface()->proposeLoadingGame();
}

void SettingsMainWindow::saveGameButtonCallback()
{
	close();
	ENGINE->windows().createAndPushWindow<CSavingScreen>();
}

void SettingsMainWindow::restartGameButtonCallback()
{
	GAME->interface()->showYesNoDialog(
		LIBRARY->generaltexth->allTexts[67],
		[this]()
		{
			close();
			ENGINE->dispatchMainThread([](){
				GAME->server().sendRestartGame();
			});
		},
		0
	);
}

void SettingsMainWindow::showAll(Canvas & to)
{
	auto color = GAME->interface() ? GAME->interface()->playerID : PlayerColor(1);
	if(settings["session"]["spectate"].Bool())
		color = PlayerColor(1); // TODO: Spectator shouldn't need special code for UI colors

	CIntObject::showAll(to);
	CMessage::drawBorder(color, to, pos.w+28, pos.h+29, pos.x-14, pos.y-15);
}

void SettingsMainWindow::onScreenResize()
{
	InterfaceObjectConfigurable::onScreenResize();

	auto tab = std::dynamic_pointer_cast<GeneralOptionsTab>(tabContentArea->getItem());

	if (tab)
		tab->updateResolutionSelector();
}
