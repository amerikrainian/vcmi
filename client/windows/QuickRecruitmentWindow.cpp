/*
 * QuickRecruitmentWindow.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "QuickRecruitmentWindow.h"
#include "../../lib/mapObjects/CGTownInstance.h"
#include "../CPlayerInterface.h"
#include "../widgets/Buttons.h"
#include "../widgets/CreatureCostBox.h"
#include "../widgets/Slider.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/Shortcut.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/ResourceSet.h"
#include "../../lib/CCreatureHandler.h"
#include "CreaturePurchaseCard.h"
#include "../gui/AccessibilityManager.h"
#include "../gui/FocusManager.h"


void QuickRecruitmentWindow::setButtons()
{
	setCancelButton();
	setBuyButton();
	setMaxButton();
}

void QuickRecruitmentWindow::setCancelButton()
{
	cancelButton = std::make_shared<CButton>(Point((pos.w / 2) + 48, 418), AnimationPath::builtin("ICN6432.DEF"), CButton::tooltip(), [&](){ close(); }, EShortcut::GLOBAL_CANCEL);
	cancelButton->setImageOrder(0, 1, 2, 3);
	cancelButton->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Cancel")
		.withDescription("Close recruitment window")
		.withTabOrder(200)
	);
}

void QuickRecruitmentWindow::setBuyButton()
{
	buyButton = std::make_shared<CButton>(Point((pos.w / 2) - 32, 418), AnimationPath::builtin("IBY6432.DEF"), CButton::tooltip(), [&](){ purchaseUnits(); }, EShortcut::GLOBAL_ACCEPT);
	buyButton->setImageOrder(0, 1, 2, 3);
	buyButton->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Buy")
		.withDescription("Purchase selected creatures")
		.withTabOrder(199)
	);
}

void QuickRecruitmentWindow::setMaxButton()
{
	maxButton = std::make_shared<CButton>(Point((pos.w/2)-112, 418), AnimationPath::builtin("IRCBTNS.DEF"), CButton::tooltip(), [&](){ maxAllCards(cards); }, EShortcut::RECRUITMENT_MAX);
	maxButton->setImageOrder(0, 1, 2, 3);
	maxButton->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Buy All")
		.withDescription("Set maximum amount for all creatures")
		.withTabOrder(198)
	);
}

void QuickRecruitmentWindow::setCreaturePurchaseCards()
{
	int availableAmount = getAvailableCreatures();
	Point position = Point((pos.w - 100*availableAmount - 8*(availableAmount-1))/2,64);
	for (int i = 0; i < town->getTown()->creatures.size(); i++)
	{
		if(!town->getTown()->creatures.at(i).empty() && !town->creatures.at(i).second.empty() && town->creatures[i].first)
		{
			cards.push_back(std::make_shared<CreaturePurchaseCard>(town->creatures[i].second, position, town->creatures[i].first, this));
			position.x += 108;
		}
	}
	totalCost = std::make_shared<CreatureCostBox>(Rect((this->pos.w/2)-45, position.y+260, 97, 74), "");
}

void QuickRecruitmentWindow::initWindow(Rect startupPosition)
{
	pos.x = startupPosition.x + 238;
	pos.y = startupPosition.y + 45;
	pos.w = 332;
	pos.h = 461;
	int creaturesAmount = getAvailableCreatures();
	if(creaturesAmount > 3)
	{
		pos.w += 108 * (creaturesAmount - 3);
		pos.x -= 55 * (creaturesAmount - 3);
	}
	backgroundTexture = std::make_shared<CFilledTexture>(ImagePath::builtin("DIBOXBCK.pcx"), Rect(0, 0, pos.w, pos.h));
	costBackground = std::make_shared<CPicture>(ImagePath::builtin("QuickRecruitmentWindow/costBackground.png"), pos.w/2-113, 335);
}

void QuickRecruitmentWindow::maxAllCards(std::vector<std::shared_ptr<CreaturePurchaseCard> > cards)
{
	auto allAvailableResources = GAME->interface()->cb->getResourceAmount();
	for(auto i : boost::adaptors::reverse(cards))
	{
		si32 maxAmount = i->creatureOnTheCard->maxAmount(allAvailableResources);
		vstd::amin(maxAmount, i->maxAmount);

		i->slider->setAmount(maxAmount);

		if(i->slider->getValue() != maxAmount)
			i->slider->scrollTo(maxAmount);
		else
			i->sliderMoved(maxAmount);

		i->slider->scrollToMax();
		allAvailableResources -= (i->creatureOnTheCard->getFullRecruitCost() * maxAmount);
	}
	maxButton->block(allAvailableResources == GAME->interface()->cb->getResourceAmount());
}


void QuickRecruitmentWindow::purchaseUnits()
{
	for(auto selected : boost::adaptors::reverse(cards))
	{
		if(selected->slider->getValue())
		{
			int level = 0;
			int i = 0;
			for(auto c : town->getTown()->creatures)
			{
				for(auto c2 : c)
					if(c2 == selected->creatureOnTheCard->getId())
						level = i;
				i++;
			}
			auto onRecruit = [this, level](CreatureID id, int count){ GAME->interface()->cb->recruitCreatures(town, town->getUpperArmy(), id, count, level); };
			CreatureID crid =  selected->creatureOnTheCard->getId();
			SlotID dstslot = town -> getSlotFor(crid);
			if(!dstslot.validSlot())
				continue;
			onRecruit(crid, selected->slider->getValue());
		}
	}
	close();
}

int QuickRecruitmentWindow::getAvailableCreatures()
{
	int creaturesAmount = 0;
	for (int i=0; i< town->getTown()->creatures.size(); i++)
		if(!town->getTown()->creatures.at(i).empty() && !town->creatures.at(i).second.empty() && town->creatures[i].first)
			creaturesAmount++;
	return creaturesAmount;
}

void QuickRecruitmentWindow::updateAllSliders()
{
	auto allAvailableResources = GAME->interface()->cb->getResourceAmount();
	for(auto i : boost::adaptors::reverse(cards))
		allAvailableResources -= (i->creatureOnTheCard->getFullRecruitCost() * i->slider->getValue());
	for(auto i : cards)
	{
		si32 maxAmount = i->creatureOnTheCard->maxAmount(allAvailableResources);
		vstd::amin(maxAmount, i->maxAmount);
		if(maxAmount < 0)
			continue;
		if(i->slider->getValue() + maxAmount < i->maxAmount)
			i->slider->setAmount(i->slider->getValue() + maxAmount);
		else
			i->slider->setAmount(i->maxAmount);
		i->slider->scrollTo(i->slider->getValue());
	}
	TResources totalCostResources = GAME->interface()->cb->getResourceAmount() - allAvailableResources;
	totalCost->createItems(totalCostResources);
	totalCost->set(totalCostResources);
	
	// Announce total cost when it changes
	if(AccessibilityManager::getInstance().isScreenReaderEnabled() && !totalCostResources.empty())
	{
		std::string announcement = "Total cost: " + totalCostResources.toHumanReadable();
		AccessibilityManager::getInstance().announce(announcement);
	}
}

QuickRecruitmentWindow::QuickRecruitmentWindow(const CGTownInstance * townd, Rect startupPosition)
	: CWindowObject(PLAYER_COLORED | BORDERED),
	town(townd),
	focusedCardIndex(-1)
{
	OBJECT_CONSTRUCTION;

	initWindow(startupPosition);
	setButtons();
	setCreaturePurchaseCards();
	maxAllCards(cards);

	center();
	
	// Set accessibility info for the dialog
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("dialog")
		.withName("Recruit Creatures")
		.withDescription("Window to recruit creatures from " + town->getNameTranslated())
	);
	
	// Announce window when opened
	AccessibilityManager::getInstance().announce("Recruit Creatures window opened. Use Tab to navigate between creatures and sliders, arrow keys to adjust quantities.", true);
	
	// Set initial focus to first card if available
	if(!cards.empty())
	{
		setFocusToCard(0);
	}
}

void QuickRecruitmentWindow::setFocusToCard(int index)
{
	if(index >= 0 && index < cards.size())
	{
		// Remove focus from previous card
		if(focusedCardIndex >= 0 && focusedCardIndex < cards.size())
		{
			// Previous card loses focus automatically
		}
		
		focusedCardIndex = index;
		cards[focusedCardIndex]->setCardFocus();
	}
}

void QuickRecruitmentWindow::keyPressed(EShortcut key)
{
	switch(key)
	{
		case EShortcut::GLOBAL_MOVE_FOCUS:
			// Move to next card
			if(focusedCardIndex < cards.size() - 1)
			{
				setFocusToCard(focusedCardIndex + 1);
			}
			else
			{
				// Move focus to buttons
				if(focusedCardIndex >= 0 && focusedCardIndex < cards.size())
				{
					// Previous card loses focus automatically
				}
				focusedCardIndex = -1;
				// TODO: Set focus to maxButton using FocusManager
				AccessibilityManager::getInstance().announce("Buy All button");
			}
			break;
			
		case EShortcut::GLOBAL_MOVE_FOCUS_PREV:
			// Move to previous card
			if(focusedCardIndex > 0)
			{
				setFocusToCard(focusedCardIndex - 1);
			}
			else if(focusedCardIndex == -1 && !cards.empty())
			{
				// Move back from buttons to last card
				setFocusToCard(cards.size() - 1);
			}
			break;
			
		default:
			CWindowObject::keyPressed(key);
			break;
	}
}

void QuickRecruitmentWindow::show(Canvas & to)
{
	CWindowObject::show(to);
	
	// Draw focus indicator for focused card
	if(focusedCardIndex >= 0 && focusedCardIndex < cards.size())
	{
		// Visual focus indicator could be drawn here if needed
		// For example, draw a highlight rectangle around the focused card
	}
}

void QuickRecruitmentWindow::activate()
{
	CWindowObject::activate();
	
	// Set initial focus to first card if available
	if(!cards.empty())
	{
		setFocusToCard(0);
	}
}