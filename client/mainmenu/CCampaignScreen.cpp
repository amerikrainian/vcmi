/*
 * CCampaignScreen.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "CCampaignScreen.h"

#include "CMainMenu.h"

#include "../CPlayerInterface.h"
#include "../CServerHandler.h"
#include "../GameEngine.h"
#include "../gui/Shortcut.h"
#include "../gui/AccessibilityManager.h"
#include "../media/IMusicPlayer.h"
#include "../render/Canvas.h"
#include "../widgets/CComponent.h"
#include "../widgets/Buttons.h"
#include "../widgets/MiscWidgets.h"
#include "../widgets/ObjectLists.h"
#include "../widgets/TextControls.h"
#include "../widgets/VideoWidget.h"
#include "../windows/GUIClasses.h"
#include "../windows/InfoWindows.h"
#include "../windows/CWindowObject.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/CCreatureHandler.h"
#include "../../lib/CSkillHandler.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/campaign/CampaignHandler.h"
#include "../../lib/filesystem/Filesystem.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/mapping/CMapService.h"
#include "../../lib/spells/CSpellHandler.h"
#include "../../lib/texts/CGeneralTextHandler.h"

CCampaignScreen::CCampaignScreen(const JsonNode & config, std::string name)
	: CWindowObject(BORDERED), campaignSet(name)
{
	OBJECT_CONSTRUCTION;
	
	// Set accessibility info for the main window
	UIAccessibilityInfo windowAccessInfo;
	windowAccessInfo.role = "window";
	windowAccessInfo.name = LIBRARY->generaltexth->translate("vcmi.mainmenu.campaignMenu");
	windowAccessInfo.description = LIBRARY->generaltexth->translate("vcmi.mainmenu.campaignMenuDescription");
	setAccessibilityInfo(windowAccessInfo);
	
	const auto& campaigns = config[name]["items"].Vector();

	// Define mapping of background name -> campaigns per page
	const std::unordered_map<std::string, int> campaignsPerPageMap = {
		{"CampaignBackground4", 4},
		{"CampaignBackground5", 5},
		{"CampaignBackground6", 6},
		{"CampaignBackground7", 7},
		{"CAMPBACK", 7},
		{"CAMPBKX2", 7},
		{"CampaignBackground8", 8}
	};

	// Process images and check if name is in the map
	for (const JsonNode& node : config[name]["images"].Vector())
	{
		images.push_back(CMainMenu::createPicture(node));

		std::string imageName = node["name"].String();
		auto it = campaignsPerPageMap.find(imageName);
		if (it != campaignsPerPageMap.end())
		{
			campaignsPerPage = it->second;
		}
	}

	if (!images.empty())
	{
		images[0]->center(); // move background to center
		moveTo(images[0]->pos.topLeft()); // move everything else to center
		images[0]->moveTo(pos.topLeft()); // restore moved twice background
		pos = images[0]->pos; // fix height\width of this window
	}
	
	int tabOrderBase = 100;
	for (size_t i = 0; i < campaigns.size(); ++i)
	{
		auto button = std::make_shared<CCampaignButton>(campaigns[i], config, campaignSet);
		button->enable();
		
		// Set tab order for campaign buttons
		if (button->getAccessibilityInfo())
		{
			UIAccessibilityInfo updatedInfo = *button->getAccessibilityInfo();
			updatedInfo.withTabOrder(tabOrderBase + i);
			button->setAccessibilityInfo(updatedInfo);
		}
		
		campButtons.push_back(button);
	}

	maxPages = (campaigns.size() + campaignsPerPage - 1) / campaignsPerPage;
	
	if (!config[name]["nextbutton"].isNull())
	{
		buttonNext = std::make_shared<CButton>(
			Point(config[name]["nextbutton"]["x"].Integer(), config[name]["nextbutton"]["y"].Integer()),
			AnimationPath::fromJson(config[name]["nextbutton"]["name"]),
			std::make_pair("", ""),
			[this, name]() { switchPage(1); }
		);
		buttonNext->setHoverable(true);
		buttonNext->disable();
		
		// Set accessibility info for next button
		UIAccessibilityInfo nextBtnInfo;
		nextBtnInfo.role = "button";
		nextBtnInfo.name = LIBRARY->generaltexth->translate("vcmi.mainmenu.nextPage");
		nextBtnInfo.description = LIBRARY->generaltexth->translate("vcmi.mainmenu.nextPageDescription");
		nextBtnInfo.tabOrder = 500;
		buttonNext->setAccessibilityInfo(nextBtnInfo);
	}

	if (!config[name]["backbutton"].isNull())
	{
		buttonPrev = std::make_shared<CButton>(
			Point(config[name]["backbutton"]["x"].Integer(), config[name]["backbutton"]["y"].Integer()),
			AnimationPath::fromJson(config[name]["backbutton"]["name"]),
			std::make_pair("", ""),
			[this, name]() { switchPage(-1); }
		);
		buttonPrev->setHoverable(true);
		buttonPrev->disable();
		
		// Set accessibility info for previous button
		UIAccessibilityInfo prevBtnInfo;
		prevBtnInfo.role = "button";
		prevBtnInfo.name = LIBRARY->generaltexth->translate("vcmi.mainmenu.prevPage");
		prevBtnInfo.description = LIBRARY->generaltexth->translate("vcmi.mainmenu.prevPageDescription");
		prevBtnInfo.tabOrder = 499;
		buttonPrev->setAccessibilityInfo(prevBtnInfo);
	}

	page = std::make_shared<CLabel>(10, 570, FONT_MEDIUM, ETextAlignment::BOTTOMLEFT, Colors::YELLOW, "");
	
	// Set accessibility info for page label
	UIAccessibilityInfo pageInfo;
	pageInfo.role = "label";
	pageInfo.name = LIBRARY->generaltexth->translate("vcmi.mainmenu.pageIndicator");
	page->setAccessibilityInfo(pageInfo);

	if (!config[name]["exitbutton"].isNull())
	{
		buttonBack = createExitButton(config[name]["exitbutton"]);
		buttonBack->setHoverable(true);
		
		// Set accessibility info for exit button
		UIAccessibilityInfo exitBtnInfo;
		exitBtnInfo.role = "button";
		exitBtnInfo.name = LIBRARY->generaltexth->translate("vcmi.mainmenu.back");
		exitBtnInfo.description = LIBRARY->generaltexth->translate("vcmi.mainmenu.backToMainMenu");
		exitBtnInfo.tabOrder = 600;
		buttonBack->setAccessibilityInfo(exitBtnInfo);
	}

	updateCampaignButtons(config);
}

void CCampaignScreen::activate()
{
	ENGINE->music().playMusic(AudioPath::builtin("Music/MainMenu"), true, false);

	CWindowObject::activate();
}

std::shared_ptr<CButton> CCampaignScreen::createExitButton(const JsonNode & button)
{
	std::pair<std::string, std::string> help;
	if(!button["help"].isNull() && button["help"].Float() > 0)
		help = LIBRARY->generaltexth->zelp[(size_t)button["help"].Float()];

	return std::make_shared<CButton>(Point((int)button["x"].Float(), (int)button["y"].Float()), AnimationPath::fromJson(button["name"]), help, [this](){ close();}, EShortcut::GLOBAL_CANCEL);
}

CCampaignScreen::CCampaignButton::CCampaignButton(const JsonNode & config, const JsonNode & parentConfig, std::string campaignSet)
	: campaignSet(campaignSet)
{
	OBJECT_CONSTRUCTION;

	pos.x += static_cast<int>(config["x"].Float());
	pos.y += static_cast<int>(config["y"].Float());
	pos.w = 200;
	pos.h = 116;

	campFile = config["file"].String();
	videoPath = VideoPath::fromJson(config["video"]);

	status = CCampaignScreen::ENABLED;
	
	// Set basic accessibility info - will be updated based on status
	UIAccessibilityInfo accessInfo;
	accessInfo.role = "listitem";
	setAccessibilityInfo(accessInfo);

	if(CResourceHandler::get()->existsResource(ResourcePath(campFile, EResType::CAMPAIGN)))
	{
		auto header = CampaignHandler::getHeader(campFile);
		hoverText = header->getNameTranslated();

		if (persistentStorage["completedCampaigns"][header->getFilename()].Bool())
			status = CCampaignScreen::COMPLETED;
			
		// Update accessibility info with campaign name
		UIAccessibilityInfo updatedInfo = *getAccessibilityInfo();
		updatedInfo.name = hoverText;
		setAccessibilityInfo(updatedInfo);
	}
	else
	{
		status = CCampaignScreen::DISABLED;
		
		// Update accessibility info for disabled campaign
		UIAccessibilityInfo updatedInfo = *getAccessibilityInfo();
		updatedInfo.name = campFile;
		updatedInfo.state = "disabled";
		setAccessibilityInfo(updatedInfo);
	}

	for(const JsonNode & node : parentConfig[campaignSet]["items"].Vector())
	{
		for(const JsonNode & requirement : config["requires"].Vector())
		{
			if(node["id"].Integer() == requirement.Integer())
				if(!persistentStorage["completedCampaigns"][node["file"].String()].Bool())
				{
					status = CCampaignScreen::DISABLED;
					
					// Update accessibility state to disabled if requirements not met
					UIAccessibilityInfo updatedInfo = *getAccessibilityInfo();
					updatedInfo.state = "disabled";
					updatedInfo.description = LIBRARY->generaltexth->translate("vcmi.mainmenu.campaignRequirementsNotMet");
					setAccessibilityInfo(updatedInfo);
				}
		}
	}

	if(persistentStorage["unlockAllCampaigns"].Bool())
		status = CCampaignScreen::ENABLED;

	if(status != CCampaignScreen::DISABLED)
	{
		addUsedEvents(LCLICK | HOVER);
		graphicsImage = std::make_shared<CPicture>(ImagePath::fromJson(config["image"]));
		hoverLabel = std::make_shared<CLabel>(pos.w / 2, pos.h + 20, FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW, "");
		parent->addChild(hoverLabel.get());
	}

	if(status == CCampaignScreen::COMPLETED)
	{
		graphicsCompleted = std::make_shared<CPicture>(ImagePath::builtin("CAMPCHK"));
		
		// Update accessibility state to completed
		UIAccessibilityInfo updatedInfo = *getAccessibilityInfo();
		updatedInfo.state = "completed";
		updatedInfo.description = LIBRARY->generaltexth->translate("vcmi.mainmenu.campaignCompleted");
		setAccessibilityInfo(updatedInfo);
	}
}

void CCampaignScreen::CCampaignButton::clickReleased(const Point & cursorPosition)
{
	CMainMenu::openCampaignLobby(campFile, campaignSet);
}

void CCampaignScreen::CCampaignButton::hover(bool on)
{
	OBJECT_CONSTRUCTION;

	if (on && !videoPath.empty())
		videoPlayer = std::make_shared<VideoWidget>(Point(), videoPath, false);
	else
		videoPlayer.reset();

	if(hoverLabel)
	{
		if(on)
			hoverLabel->setText(hoverText); // Shows the name of the campaign when you get into the bounds of the button
		else
			hoverLabel->setText(" ");
	}
}

bool CCampaignScreen::CCampaignButton::isFocusable() const
{
	return status != CCampaignScreen::DISABLED;
}

void CCampaignScreen::switchPage(int delta)
{
	currentPage += delta;
	currentPage = std::clamp(currentPage, 0, maxPages - 1);

	const auto& campaignConfig = CMainMenuConfig::get().getCampaigns();

	updateCampaignButtons(campaignConfig);
}

void CCampaignScreen::updateCampaignButtons(const JsonNode & parentConfig)
{
	const auto& campaigns = parentConfig[campaignSet]["items"].Vector();

	int minId = (currentPage * campaignsPerPage) + 1;
	int maxId = minId + campaignsPerPage - 1;

	for(size_t i = 0; i < campButtons.size(); ++i)
	{
		int campaignId = campaigns[i]["id"].Integer();

		if(campaignId >= minId && campaignId <= maxId)
			campButtons[i]->enable();
		else
			campButtons[i]->disable();

		if(!CResourceHandler::get()->existsResource(ResourcePath(campaigns[i]["file"].String(), EResType::CAMPAIGN)))
		{
			campButtons[i]->disable();
			logGlobal->warn("Campaign %s doesn't exist", campaigns[i]["file"].String());
		}
	}

	if(buttonNext && buttonPrev)
	{
		page->setText(std::to_string(currentPage + 1) + "/" + std::to_string(maxPages));

		if (maxId < campaigns.size())
			buttonNext->enable();
		else
			buttonNext->disable();

		if (currentPage > 0)
			buttonPrev->enable();
		else
			buttonPrev->disable();
	}

	redraw();
}
