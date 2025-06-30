/*
 * VcmiSettingsWindow.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"

#include "OtherOptionsTab.h"

#include "../../../lib/filesystem/ResourcePath.h"
#include "../../GameEngine.h"
#include "../../gui/AccessibilityManager.h"
#include "../../widgets/Buttons.h"
#include "CConfigHandler.h"

static void setBoolSetting(std::string group, std::string field, bool value)
{
	Settings fullscreen = settings.write[group][field];
	fullscreen->Bool() = value;
}

OtherOptionsTab::OtherOptionsTab() : InterfaceObjectConfigurable()
{
	OBJECT_CONSTRUCTION;

	const JsonNode config(JsonPath::builtin("config/widgets/settings/otherOptionsTab.json"));
	addCallback("availableCreaturesAsDwellingLabelChanged", [](bool value)
	{
		return setBoolSetting("gameTweaks", "availableCreaturesAsDwellingLabel", value);
	});
	addCallback("compactTownCreatureInfoChanged", [](bool value)
	{
		return setBoolSetting("gameTweaks", "compactTownCreatureInfo", value);
	});
	build(config);

	// Available creatures as dwelling label checkbox
	std::shared_ptr<CToggleButton> availableCreaturesAsDwellingLabelCheckbox = widget<CToggleButton>("availableCreaturesAsDwellingLabelCheckbox");
	availableCreaturesAsDwellingLabelCheckbox->setSelected(settings["gameTweaks"]["availableCreaturesAsDwellingLabel"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Show creatures as dwelling icons";
		accessInfo.description = "Display available creatures as dwelling building icons instead of numbers";
		accessInfo.state = availableCreaturesAsDwellingLabelCheckbox->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 1;
		availableCreaturesAsDwellingLabelCheckbox->setAccessibilityInfo(accessInfo);
	}

	// Compact town creature info checkbox
	std::shared_ptr<CToggleButton> compactTownCreatureInfo = widget<CToggleButton>("compactTownCreatureInfoCheckbox");
	compactTownCreatureInfo->setSelected(settings["gameTweaks"]["compactTownCreatureInfo"].Bool());
	{
		UIAccessibilityInfo accessInfo;
		accessInfo.role = "checkbox";
		accessInfo.name = "Compact creature info";
		accessInfo.description = "Use compact display for creature information in town screen";
		accessInfo.state = compactTownCreatureInfo->isSelected() ? "checked" : "";
		accessInfo.tabOrder = 2;
		compactTownCreatureInfo->setAccessibilityInfo(accessInfo);
	}
}


