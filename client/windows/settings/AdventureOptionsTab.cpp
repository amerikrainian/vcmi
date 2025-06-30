/*
 * AdventureOptionsTab.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "AdventureOptionsTab.h"

#include "../../GameEngine.h"
#include "../../eventsSDL/InputHandler.h"
#include "../../gui/WindowHandler.h"
#include "../../gui/AccessibilityManager.h"
#include "../../widgets/Buttons.h"
#include "../../widgets/Images.h"
#include "../../widgets/TextControls.h"

#include "../../../lib/CConfigHandler.h"
#include "../../../lib/filesystem/ResourcePath.h"

static void setBoolSetting(std::string group, std::string field, bool value)
{
	Settings fullscreen = settings.write[group][field];
	fullscreen->Bool() = value;
}

static void setIntSetting(std::string group, std::string field, int value)
{
	Settings entry = settings.write[group][field];
	entry->Float() = value;
}

AdventureOptionsTab::AdventureOptionsTab()
		: InterfaceObjectConfigurable()
{
	OBJECT_CONSTRUCTION;
	setRedrawParent(true);

	addConditional("touchscreen", ENGINE->input().getCurrentInputMode() == InputMode::TOUCH);
	addConditional("keyboardMouse", ENGINE->input().getCurrentInputMode() == InputMode::KEYBOARD_AND_MOUSE);
	addConditional("controller", ENGINE->input().getCurrentInputMode() == InputMode::CONTROLLER);
#ifdef VCMI_MOBILE
	addConditional("mobile", true);
	addConditional("desktop", false);
#else
	addConditional("mobile", false);
	addConditional("desktop", true);
#endif

	const JsonNode config(JsonPath::builtin("config/widgets/settings/adventureOptionsTab.json"));
	addCallback("playerHeroSpeedChanged", [this](int value)
	{
		auto targetLabel = widget<CLabel>("heroSpeedValueLabel");
		if (targetLabel)
		{
			if (value <= 0)
			{
				targetLabel->setText("-");
			}
			else
			{
				int valuePercentage = 100 * 100 / value;
				targetLabel->setText(std::to_string(valuePercentage) + "%");
			}
		}
		setIntSetting("adventure", "heroMoveTime", value);
	});
	addCallback("enemyHeroSpeedChanged", [this](int value)
	{
		auto targetLabel = widget<CLabel>("enemySpeedValueLabel");

		if (targetLabel)
		{
			if (value <= 0)
			{
				targetLabel->setText("-");
			}
			else
			{
				int valuePercentage = 100 * 100 / value;
				targetLabel->setText(std::to_string(valuePercentage) + "%");
			}
		}
		setIntSetting("adventure", "enemyMoveTime", value);
	});
	addCallback("mapScrollSpeedChanged", [this](int value)
	{
		auto targetLabel = widget<CLabel>("mapScrollingValueLabel");
		int valuePercentage = 100 * value / 1200; // H3 max value is "1200", displaying it to be 100%
		if (targetLabel)
			targetLabel->setText(std::to_string(valuePercentage) + "%");

		return setIntSetting("adventure", "scrollSpeedPixels", value);
	});
	addCallback("heroReminderChanged", [](bool value)
	{
		return setBoolSetting("adventure", "heroReminder", value);
	});
	addCallback("quickCombatChanged", [](bool value)
	{
		return setBoolSetting("adventure", "quickCombat", value);
	});
	//settings that do not belong to base game:
	addCallback("numericQuantitiesChanged", [](bool value)
	{
		return setBoolSetting("gameTweaks", "numericCreaturesQuantities", value);
	});
	addCallback("forceMovementInfoChanged", [](bool value)
	{
		return setBoolSetting("gameTweaks", "forceMovementInfo", value);
	});
	addCallback("showGridChanged", [](bool value)
	{
		return setBoolSetting("gameTweaks", "showGrid", value);
	});
	addCallback("infoBarPickChanged", [](bool value)
	{
		return setBoolSetting("gameTweaks", "infoBarPick", value);
	});
	addCallback("borderScrollChanged", [](bool value)
	{
		return setBoolSetting("adventure", "borderScroll", value);
	});
	addCallback("infoBarCreatureManagementChanged", [](bool value)
	{
		return setBoolSetting("gameTweaks", "infoBarCreatureManagement", value);
	});
	addCallback("leftButtonDragChanged", [](bool value)
	{
		return setBoolSetting("adventure", "leftButtonDrag", value);
	});
	addCallback("rightButtonDragChanged", [](bool value)
	{
		return setBoolSetting("adventure", "rightButtonDrag", value);
	});
	addCallback("smoothDraggingChanged", [](bool value)
	{
		return setBoolSetting("adventure", "smoothDragging", value);
	});
	addCallback("skipAdventureMapAnimationsChanged", [](bool value)
	{
		return setBoolSetting("gameTweaks", "skipAdventureMapAnimations", value);
	});
	addCallback("hideBackgroundChanged", [](bool value)
	{
		return setBoolSetting("adventure", "hideBackground", value);
	});
	addCallback("minimapShowHeroesChanged", [](bool value)
	{
		setBoolSetting("adventure", "minimapShowHeroes", value);
		ENGINE->windows().totalRedraw();
	});
	build(config);

	// Initialize settings and add accessibility information
	std::shared_ptr<CToggleGroup> playerHeroSpeedToggle = widget<CToggleGroup>("heroMovementSpeedPicker");
	playerHeroSpeedToggle->setSelected(static_cast<int>(settings["adventure"]["heroMoveTime"].Float()));
	// Add accessibility to hero speed buttons
	for (auto& [index, button] : playerHeroSpeedToggle->buttons)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "radio";
		switch(index)
		{
			case 200: accessInfo.name = "Very slow hero speed"; accessInfo.tabOrder = 1; break;
			case 150: accessInfo.name = "Slow hero speed"; accessInfo.tabOrder = 2; break;
			case 100: accessInfo.name = "Normal hero speed"; accessInfo.tabOrder = 3; break;
			case 50: accessInfo.name = "Fast hero speed"; accessInfo.tabOrder = 4; break;
			case 25: accessInfo.name = "Very fast hero speed"; accessInfo.tabOrder = 5; break;
			case 0: accessInfo.name = "Instant hero movement"; accessInfo.tabOrder = 6; break;
			default: accessInfo.name = "Hero speed option"; break;
		}
		accessInfo.description = "Set movement speed for your heroes";
		if (auto toggleButton = std::dynamic_pointer_cast<CToggleButton>(button))
			toggleButton->setAccessibilityInfo(accessInfo);
	}

	std::shared_ptr<CToggleGroup> enemyHeroSpeedToggle = widget<CToggleGroup>("enemyMovementSpeedPicker");
	enemyHeroSpeedToggle->setSelected(static_cast<int>(settings["adventure"]["enemyMoveTime"].Float()));
	// Add accessibility to enemy speed buttons
	for (auto& [index, button] : enemyHeroSpeedToggle->buttons)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "radio";
		switch(index)
		{
			case 150: accessInfo.name = "Slow enemy speed"; accessInfo.tabOrder = 7; break;
			case 100: accessInfo.name = "Normal enemy speed"; accessInfo.tabOrder = 8; break;
			case 50: accessInfo.name = "Fast enemy speed"; accessInfo.tabOrder = 9; break;
			case 25: accessInfo.name = "Very fast enemy speed"; accessInfo.tabOrder = 10; break;
			case 0: accessInfo.name = "Instant enemy movement"; accessInfo.tabOrder = 11; break;
			case -1: accessInfo.name = "Don't show enemy movement"; accessInfo.tabOrder = 12; break;
			default: accessInfo.name = "Enemy speed option"; break;
		}
		accessInfo.description = "Set movement speed for enemy heroes";
		if (auto toggleButton = std::dynamic_pointer_cast<CToggleButton>(button))
			toggleButton->setAccessibilityInfo(accessInfo);
	}

	std::shared_ptr<CToggleGroup> mapScrollSpeedToggle = widget<CToggleGroup>("mapScrollSpeedPicker");
	mapScrollSpeedToggle->setSelected(static_cast<int>(settings["adventure"]["scrollSpeedPixels"].Float()));
	// Add accessibility to map scroll speed buttons
	for (auto& [index, button] : mapScrollSpeedToggle->buttons)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "radio";
		switch(index)
		{
			case 200: accessInfo.name = "Very slow scrolling"; accessInfo.tabOrder = 13; break;
			case 400: accessInfo.name = "Slow scrolling"; accessInfo.tabOrder = 14; break;
			case 800: accessInfo.name = "Normal scrolling"; accessInfo.tabOrder = 15; break;
			case 1200: accessInfo.name = "Fast scrolling"; accessInfo.tabOrder = 16; break;
			case 2400: accessInfo.name = "Very fast scrolling"; accessInfo.tabOrder = 17; break;
			case 4800: accessInfo.name = "Ultra fast scrolling"; accessInfo.tabOrder = 18; break;
			default: accessInfo.name = "Scrolling speed option"; break;
		}
		accessInfo.description = "Set map scrolling speed";
		if (auto toggleButton = std::dynamic_pointer_cast<CToggleButton>(button))
			toggleButton->setAccessibilityInfo(accessInfo);
	}

	// Hero reminder checkbox
	std::shared_ptr<CToggleButton> heroReminderCheckbox = widget<CToggleButton>("heroReminderCheckbox");
	heroReminderCheckbox->setSelected(settings["adventure"]["heroReminder"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Hero reminder";
		accessInfo.description = "Show reminder when hero has movement points remaining";
		accessInfo.state = heroReminderCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 19;
		heroReminderCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Quick combat checkbox
	std::shared_ptr<CToggleButton> quickCombatCheckbox = widget<CToggleButton>("quickCombatCheckbox");
	quickCombatCheckbox->setSelected(settings["adventure"]["quickCombat"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Quick combat";
		accessInfo.description = "Skip combat animations and resolve battles instantly";
		accessInfo.state = quickCombatCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 20;
		quickCombatCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Numeric quantities checkbox
	std::shared_ptr<CToggleButton> numericQuantitiesCheckbox = widget<CToggleButton>("numericQuantitiesCheckbox");
	numericQuantitiesCheckbox->setSelected(settings["gameTweaks"]["numericCreaturesQuantities"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Numeric creature quantities";
		accessInfo.description = "Show exact creature numbers instead of descriptive text";
		accessInfo.state = numericQuantitiesCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 24;
		numericQuantitiesCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Force movement info checkbox
	std::shared_ptr<CToggleButton> forceMovementInfoCheckbox = widget<CToggleButton>("forceMovementInfoCheckbox");
	forceMovementInfoCheckbox->setSelected(settings["gameTweaks"]["forceMovementInfo"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Force movement info";
		accessInfo.description = "Always show movement range and path for heroes";
		accessInfo.state = forceMovementInfoCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 25;
		forceMovementInfoCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Show grid checkbox
	std::shared_ptr<CToggleButton> showGridCheckbox = widget<CToggleButton>("showGridCheckbox");
	showGridCheckbox->setSelected(settings["gameTweaks"]["showGrid"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Show grid";
		accessInfo.description = "Display grid overlay on adventure map";
		accessInfo.state = showGridCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 21;
		showGridCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Info bar pick checkbox
	std::shared_ptr<CToggleButton> infoBarPickCheckbox = widget<CToggleButton>("infoBarPickCheckbox");
	infoBarPickCheckbox->setSelected(settings["gameTweaks"]["infoBarPick"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Info bar creature pick";
		accessInfo.description = "Allow picking creatures from info bar";
		accessInfo.state = infoBarPickCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 27;
		infoBarPickCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Border scroll checkbox
	std::shared_ptr<CToggleButton> borderScrollCheckbox = widget<CToggleButton>("borderScrollCheckbox");
	borderScrollCheckbox->setSelected(settings["adventure"]["borderScroll"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Border scrolling";
		accessInfo.description = "Scroll map when mouse is at screen edge";
		accessInfo.state = borderScrollCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 28;
		borderScrollCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Info bar creature management checkbox
	std::shared_ptr<CToggleButton> infoBarCreatureManagementCheckbox = widget<CToggleButton>("infoBarCreatureManagementCheckbox");
	infoBarCreatureManagementCheckbox->setSelected(settings["gameTweaks"]["infoBarCreatureManagement"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Info bar creature management";
		accessInfo.description = "Enable creature management features in info bar";
		accessInfo.state = infoBarCreatureManagementCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 29;
		infoBarCreatureManagementCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Left button drag checkbox
	std::shared_ptr<CToggleButton> leftButtonDragCheckbox = widget<CToggleButton>("leftButtonDragCheckbox");
	if (leftButtonDragCheckbox)
	{
		leftButtonDragCheckbox->setSelected(settings["adventure"]["leftButtonDrag"].Bool());
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Left button drag";
		accessInfo.description = "Enable map dragging with left mouse button";
		accessInfo.state = leftButtonDragCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 30;
		leftButtonDragCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Right button drag checkbox
	std::shared_ptr<CToggleButton> rightButtonDragCheckbox = widget<CToggleButton>("rightButtonDragCheckbox");
	if (rightButtonDragCheckbox)
	{
		rightButtonDragCheckbox->setSelected(settings["adventure"]["rightButtonDrag"].Bool());
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Right button drag";
		accessInfo.description = "Enable map dragging with right mouse button";
		accessInfo.state = rightButtonDragCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 32;
		rightButtonDragCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Smooth dragging checkbox
	std::shared_ptr<CToggleButton> smoothDraggingCheckbox = widget<CToggleButton>("smoothDraggingCheckbox");
	if (smoothDraggingCheckbox)
	{
		smoothDraggingCheckbox->setSelected(settings["adventure"]["smoothDragging"].Bool());
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Smooth dragging";
		accessInfo.description = "Enable smooth map dragging animation";
		accessInfo.state = smoothDraggingCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 31;
		smoothDraggingCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Skip adventure map animations checkbox
	std::shared_ptr<CToggleButton> skipAdventureMapAnimationsCheckbox = widget<CToggleButton>("skipAdventureMapAnimationsCheckbox");
	skipAdventureMapAnimationsCheckbox->setSelected(settings["gameTweaks"]["skipAdventureMapAnimations"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Skip adventure map animations";
		accessInfo.description = "Skip animations on the adventure map for faster gameplay";
		accessInfo.state = skipAdventureMapAnimationsCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 26;
		skipAdventureMapAnimationsCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Hide background checkbox
	std::shared_ptr<CToggleButton> hideBackgroundCheckbox = widget<CToggleButton>("hideBackgroundCheckbox");
	hideBackgroundCheckbox->setSelected(settings["adventure"]["hideBackground"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Hide background";
		accessInfo.description = "Hide decorative background on adventure map";
		accessInfo.state = hideBackgroundCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 22;
		hideBackgroundCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Minimap show heroes checkbox
	std::shared_ptr<CToggleButton> minimapShowHeroesCheckbox = widget<CToggleButton>("minimapShowHeroesCheckbox");
	minimapShowHeroesCheckbox->setSelected(settings["adventure"]["minimapShowHeroes"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Show heroes on minimap";
		accessInfo.description = "Display hero icons on the minimap";
		accessInfo.state = minimapShowHeroesCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 23;
		minimapShowHeroesCheckbox->setAccessibilityInfo(accessInfo);
	}
}
