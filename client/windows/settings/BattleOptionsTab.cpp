/*
 * BattleOptionsTab.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "BattleOptionsTab.h"

#include "../../battle/BattleInterface.h"
#include "../../GameEngine.h"
#include "../../../lib/CConfigHandler.h"
#include "../../../lib/filesystem/ResourcePath.h"
#include "../../../lib/texts/CGeneralTextHandler.h"
#include "../../gui/AccessibilityManager.h"
#include "../../widgets/Buttons.h"
#include "../../widgets/TextControls.h"

BattleOptionsTab::BattleOptionsTab(BattleInterface * owner)
{
	OBJECT_CONSTRUCTION;
	setRedrawParent(true);

	const JsonNode config(JsonPath::builtin("config/widgets/settings/battleOptionsTab.json"));
	addCallback("viewGridChanged", [this, owner](bool value)
	{
		viewGridChangedCallback(value, owner);
	});
	addCallback("movementShadowChanged", [this, owner](bool value)
	{
		movementShadowChangedCallback(value, owner);
	});
	addCallback("movementHighlightOnHoverChanged", [this, owner](bool value)
	{
		movementHighlightOnHoverChangedCallback(value, owner);
	});
	addCallback("rangeLimitHighlightOnHoverChanged", [this, owner](bool value)
	{
		rangeLimitHighlightOnHoverChangedCallback(value, owner);
	});
	addCallback("mouseShadowChanged", [this](bool value)
	{
		mouseShadowChangedCallback(value);
	});
	addCallback("animationSpeedChanged", [this](int value)
	{
		animationSpeedChangedCallback(value);
	});
	addCallback("showQueueChanged", [this, owner](bool value)
	{
		showQueueChangedCallback(value, owner);
	});
	addCallback("queueSizeChanged", [this, owner](int value)
	{
		queueSizeChangedCallback(value, owner);
	});
	addCallback("skipBattleIntroMusicChanged", [this](bool value)
	{
		skipBattleIntroMusicChangedCallback(value);
	});
	addCallback("showStickyHeroWindowsChanged", [this, owner](bool value)
	{
		showStickyHeroWindowsChangedCallback(value, owner);
	});
	addCallback("showQuickSpellChanged", [this, owner](bool value)
	{
		showQuickSpellChangedCallback(value, owner);
	});
	addCallback("enableAutocombatSpellsChanged", [this](bool value)
	{
		enableAutocombatSpellsChangedCallback(value);
	});
	addCallback("endWithAutocombatChanged", [this](bool value)
	{
		endWithAutocombatChangedCallback(value);
	});
	addCallback("showHealthBarChanged", [this, owner](bool value)
	{
		showHealthBarCallback(value, owner);
	});
	build(config);

	// Animation speed toggle group
	std::shared_ptr<CToggleGroup> animationSpeedToggle = widget<CToggleGroup>("animationSpeedPicker");
	animationSpeedToggle->setSelected(getAnimSpeed());
	// Add accessibility to animation speed buttons
	for (auto& [index, button] : animationSpeedToggle->buttons)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "radio";
		switch(index)
		{
			case 0: accessInfo.name = "No animations"; break;
			case 1: accessInfo.name = "Slow animations"; break;
			case 2: accessInfo.name = "Normal animations"; break;
			case 3: accessInfo.name = "Fast animations"; break;
			default: accessInfo.name = "Animation speed option"; break;
		}
		accessInfo.description = "Set speed for battle animations";
		accessInfo.tabOrder = index + 1; // Tab order 1-4
		if (auto toggleButton = std::dynamic_pointer_cast<CToggleButton>(button))
			toggleButton->setAccessibilityInfo(accessInfo);
	}

	// Queue size toggle group
	std::shared_ptr<CToggleGroup> queueSizeToggle = widget<CToggleGroup>("queueSizePicker");
	queueSizeToggle->setSelected(getQueueSizeId());
	// Add accessibility to queue size buttons
	for (auto& [index, button] : queueSizeToggle->buttons)
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "radio";
		switch(index)
		{
			case -1: accessInfo.name = "No queue"; break;
			case 0: accessInfo.name = "Auto queue size"; break;
			case 1: accessInfo.name = "Small queue"; break;
			case 2: accessInfo.name = "Large queue"; break;
			default: accessInfo.name = "Queue size option"; break;
		}
		accessInfo.description = "Set size of the battle turn queue display";
		accessInfo.tabOrder = index + 5; // Tab order 4-8 (note -1 becomes 4)
		if (auto toggleButton = std::dynamic_pointer_cast<CToggleButton>(button))
			toggleButton->setAccessibilityInfo(accessInfo);
	}

	// View grid checkbox
	std::shared_ptr<CToggleButton> viewGridCheckbox = widget<CToggleButton>("viewGridCheckbox");
	viewGridCheckbox->setSelected(settings["battle"]["cellBorders"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Show grid";
		accessInfo.description = "Display hexagonal grid on battlefield";
		accessInfo.state = viewGridCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 9;
		viewGridCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Movement shadow checkbox
	std::shared_ptr<CToggleButton> movementShadowCheckbox = widget<CToggleButton>("movementShadowCheckbox");
	movementShadowCheckbox->setSelected(settings["battle"]["stackRange"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Movement shadow";
		accessInfo.description = "Show movement range for selected stack";
		accessInfo.state = movementShadowCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 10;
		movementShadowCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Movement highlight on hover checkbox
	std::shared_ptr<CToggleButton> movementHighlightOnHoverCheckbox = widget<CToggleButton>("movementHighlightOnHoverCheckbox");
	movementHighlightOnHoverCheckbox->setSelected(settings["battle"]["movementHighlightOnHover"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Movement highlight on hover";
		accessInfo.description = "Highlight movement range when hovering over stacks";
		accessInfo.state = movementHighlightOnHoverCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 11;
		movementHighlightOnHoverCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Range limit highlight on hover checkbox
	std::shared_ptr<CToggleButton> rangeLimitHighlightOnHoverCheckbox = widget<CToggleButton>("rangeLimitHighlightOnHoverCheckbox");
	rangeLimitHighlightOnHoverCheckbox->setSelected(settings["battle"]["rangeLimitHighlightOnHover"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Range limit highlight on hover";
		accessInfo.description = "Highlight attack range limits when hovering";
		accessInfo.state = rangeLimitHighlightOnHoverCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 12;
		rangeLimitHighlightOnHoverCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Show sticky hero info windows checkbox
	std::shared_ptr<CToggleButton> showStickyHeroInfoWindowsCheckbox = widget<CToggleButton>("showStickyHeroInfoWindowsCheckbox");
	showStickyHeroInfoWindowsCheckbox->setSelected(settings["battle"]["stickyHeroInfoWindows"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Sticky hero windows";
		accessInfo.description = "Keep hero information windows always visible";
		accessInfo.state = showStickyHeroInfoWindowsCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 13;
		showStickyHeroInfoWindowsCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Show quick spell checkbox
	std::shared_ptr<CToggleButton> showQuickSpellCheckbox = widget<CToggleButton>("showQuickSpellCheckbox");
	showQuickSpellCheckbox->setSelected(settings["battle"]["enableQuickSpellPanel"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Quick spell panel";
		accessInfo.description = "Show quick spell casting panel in battle";
		accessInfo.state = showQuickSpellCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 14;
		showQuickSpellCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Mouse shadow checkbox
	std::shared_ptr<CToggleButton> mouseShadowCheckbox = widget<CToggleButton>("mouseShadowCheckbox");
	mouseShadowCheckbox->setSelected(settings["battle"]["mouseShadow"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Mouse shadow";
		accessInfo.description = "Show shadow under mouse cursor";
		accessInfo.state = mouseShadowCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 15;
		mouseShadowCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Skip battle intro music checkbox
	std::shared_ptr<CToggleButton> skipBattleIntroMusicCheckbox = widget<CToggleButton>("skipBattleIntroMusicCheckbox");
	skipBattleIntroMusicCheckbox->setSelected(settings["gameTweaks"]["skipBattleIntroMusic"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Skip battle intro music";
		accessInfo.description = "Skip intro music when battle starts";
		accessInfo.state = skipBattleIntroMusicCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 16;
		skipBattleIntroMusicCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Enable autocombat spells checkbox
	std::shared_ptr<CToggleButton> enableAutocombatSpellsCheckbox = widget<CToggleButton>("enableAutocombatSpellsCheckbox");
	enableAutocombatSpellsCheckbox->setSelected(settings["battle"]["enableAutocombatSpells"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Autocombat spells";
		accessInfo.description = "Allow AI to cast spells during autocombat";
		accessInfo.state = enableAutocombatSpellsCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 17;
		enableAutocombatSpellsCheckbox->setAccessibilityInfo(accessInfo);
	}

	// End with autocombat checkbox
	std::shared_ptr<CToggleButton> endWithAutocombatCheckbox = widget<CToggleButton>("endWithAutocombatCheckbox");
	endWithAutocombatCheckbox->setSelected(settings["battle"]["endWithAutocombat"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Autocombat until end";
		accessInfo.description = "Continue autocombat until battle ends";
		accessInfo.state = endWithAutocombatCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 18;
		endWithAutocombatCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Show health bar checkbox
	std::shared_ptr<CToggleButton> showHealthBarCheckbox = widget<CToggleButton>("showHealthBarCheckbox");
	showHealthBarCheckbox->setSelected(settings["battle"]["showHealthBar"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Show health bars";
		accessInfo.description = "Display health bars above creatures";
		accessInfo.state = showHealthBarCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 19;
		showHealthBarCheckbox->setAccessibilityInfo(accessInfo);
	}
}

int BattleOptionsTab::getAnimSpeed() const
{
	if(settings["session"]["spectate"].Bool() && !settings["session"]["spectate-battle-speed"].isNull())
		return static_cast<int>(std::round(settings["session"]["spectate-battle-speed"].Float()));

	return static_cast<int>(std::round(settings["battle"]["speedFactor"].Float()));
}

int BattleOptionsTab::getQueueSizeId() const
{
	std::string sizeText = settings["battle"]["queueSize"].String();
	bool visible = settings["battle"]["showQueue"].Bool();

	if(!visible)
		return -1;

	if(sizeText == "none")
		return -1;
	if(sizeText == "auto")
		return 0;
	if(sizeText == "small")
		return 1;
	if(sizeText == "big")
		return 2;

	return 0;
}

std::string BattleOptionsTab::getQueueSizeStringFromId(int value) const
{
	switch(value)
	{
		case -1:
			return "none";
		case 0:
			return "auto";
		case 1:
			return "small";
		case 2:
			return "big";
		default:
			return "auto";
	}
}

void BattleOptionsTab::viewGridChangedCallback(bool value, BattleInterface * parentBattleInterface)
{
	Settings cellBorders = settings.write["battle"]["cellBorders"];
	cellBorders->Bool() = value;
	if(parentBattleInterface)
		parentBattleInterface->redrawBattlefield();
}

void BattleOptionsTab::movementShadowChangedCallback(bool value, BattleInterface * parentBattleInterface)
{
	Settings stackRange = settings.write["battle"]["stackRange"];
	stackRange->Bool() = value;
	if(parentBattleInterface)
		parentBattleInterface->redrawBattlefield();
}

void BattleOptionsTab::movementHighlightOnHoverChangedCallback(bool value, BattleInterface * parentBattleInterface)
{
	Settings stackRange = settings.write["battle"]["movementHighlightOnHover"];
	stackRange->Bool() = value;
	if(parentBattleInterface)
		parentBattleInterface->redrawBattlefield();
}

void BattleOptionsTab::rangeLimitHighlightOnHoverChangedCallback(bool value, BattleInterface * parentBattleInterface)
{
	Settings stackRange = settings.write["battle"]["rangeLimitHighlightOnHover"];
	stackRange->Bool() = value;
	if(parentBattleInterface)
		parentBattleInterface->redrawBattlefield();
}

void BattleOptionsTab::mouseShadowChangedCallback(bool value)
{
	Settings shadow = settings.write["battle"]["mouseShadow"];
	shadow->Bool() = value;
}

void BattleOptionsTab::animationSpeedChangedCallback(int value)
{
	Settings speed = settings.write["battle"]["speedFactor"];
	speed->Float() = static_cast<float>(value);

	auto targetLabel = widget<CLabel>("animationSpeedValueLabel");
	int valuePercentage = value * 100 / 3; // H3 max value is "3", displaying it to be 100%
	if (targetLabel)
		targetLabel->setText(std::to_string(valuePercentage) + "%");
}

void BattleOptionsTab::showQueueChangedCallback(bool value, BattleInterface * parentBattleInterface)
{
	if(!parentBattleInterface)
	{
		Settings showQueue = settings.write["battle"]["showQueue"];
		showQueue->Bool() = value;
	}
	else
	{
		parentBattleInterface->setBattleQueueVisibility(value);
	}
}

void BattleOptionsTab::showStickyHeroWindowsChangedCallback(bool value, BattleInterface * parentBattleInterface)
{
	if(!parentBattleInterface)
	{
		Settings showStickyWindows = settings.write["battle"]["stickyHeroInfoWindows"];
		showStickyWindows->Bool() = value;
	}
	else
	{
		parentBattleInterface->setStickyHeroWindowsVisibility(value);
	}
}

void BattleOptionsTab::showQuickSpellChangedCallback(bool value, BattleInterface * parentBattleInterface)
{
	if(!parentBattleInterface)
	{
		Settings showQuickSpell = settings.write["battle"]["enableQuickSpellPanel"];
		showQuickSpell->Bool() = value;
	}
	else
	{
		parentBattleInterface->setStickyQuickSpellWindowVisibility(value);
	}
}

void BattleOptionsTab::queueSizeChangedCallback(int value, BattleInterface * parentBattleInterface)
{
	if (value == -1)
	{
		showQueueChangedCallback(false, parentBattleInterface);
		return;
	}

	std::string stringifiedValue = getQueueSizeStringFromId(value);
	Settings size = settings.write["battle"]["queueSize"];
	size->String() = stringifiedValue;

	showQueueChangedCallback(true, parentBattleInterface);
}

void BattleOptionsTab::skipBattleIntroMusicChangedCallback(bool value)
{
	Settings musicSkipSettingValue = settings.write["gameTweaks"]["skipBattleIntroMusic"];
	musicSkipSettingValue->Bool() = value;
}

void BattleOptionsTab::enableAutocombatSpellsChangedCallback(bool value)
{
	Settings enableAutocombatSpells = settings.write["battle"]["enableAutocombatSpells"];
	enableAutocombatSpells->Bool() = value;
}

void BattleOptionsTab::endWithAutocombatChangedCallback(bool value)
{
	Settings endWithAutocombat = settings.write["battle"]["endWithAutocombat"];
	endWithAutocombat->Bool() = value;
}

void BattleOptionsTab::showHealthBarCallback(bool value, BattleInterface * parentBattleInterface)
{
	Settings showHealthBar = settings.write["battle"]["showHealthBar"];
	showHealthBar->Bool() = value;
	if(parentBattleInterface)
		parentBattleInterface->redrawBattlefield();
}
