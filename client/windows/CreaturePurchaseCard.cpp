/*
 * CreaturePurchaseCard.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "CreaturePurchaseCard.h"

#include "CHeroWindow.h"
#include "QuickRecruitmentWindow.h"
#include "CCreatureWindow.h"

#include "../GameEngine.h"
#include "../gui/Shortcut.h"
#include "../gui/TextAlignment.h"
#include "../gui/WindowHandler.h"
#include "../widgets/Buttons.h"
#include "../widgets/Slider.h"
#include "../widgets/TextControls.h"
#include "../widgets/CreatureCostBox.h"
#include "../gui/FocusManager.h"
#include "../eventsSDL/InputHandler.h"

#include "../../lib/CCreatureHandler.h"
#include "../gui/AccessibilityManager.h"
#include "../gui/FocusManager.h"

void CreaturePurchaseCard::initButtons()
{
	initMaxButton();
	initMinButton();
	initCreatureSwitcherButton();
}

void CreaturePurchaseCard::initMaxButton()
{
	maxButton = std::make_shared<CButton>(Point(pos.x + 52, pos.y + 180), AnimationPath::builtin("QuickRecruitmentWindow/QuickRecruitmentAllButton.def"), CButton::tooltip(), std::bind(&CSlider::scrollToMax,slider), EShortcut::RECRUITMENT_MAX);
	maxButton->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Maximum")
		.withDescription("Set to maximum available amount")
		.withTabOrder(parent->getCardsCount() * 10 + 3)
	);
}

void CreaturePurchaseCard::initMinButton()
{
	minButton = std::make_shared<CButton>(Point(pos.x, pos.y + 180), AnimationPath::builtin("QuickRecruitmentWindow/QuickRecruitmentNoneButton.def"), CButton::tooltip(), std::bind(&CSlider::scrollToMin,slider), EShortcut::RECRUITMENT_MIN);
	minButton->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("None")
		.withDescription("Set to zero")
		.withTabOrder(parent->getCardsCount() * 10 + 2)
	);
}

void CreaturePurchaseCard::initCreatureSwitcherButton()
{
	creatureSwitcher = std::make_shared<CButton>(Point(pos.x + 18, pos.y-37), AnimationPath::builtin("iDv6432.def"), CButton::tooltip(), [&](){ switchCreatureLevel(); }, EShortcut::RECRUITMENT_SWITCH_LEVEL);
	if(upgradesID.size() > 1)
	{
		creatureSwitcher->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("button")
			.withName("Switch creature")
			.withDescription("Switch between creature upgrades")
			.withTabOrder(parent->getCardsCount() * 10 + 4)
		);
	}
}

void CreaturePurchaseCard::switchCreatureLevel()
{
	OBJECT_CONSTRUCTION;
	auto index = vstd::find_pos(upgradesID, creatureOnTheCard->getId());
	auto nextCreatureId = vstd::circularAt(upgradesID, ++index);
	creatureOnTheCard = nextCreatureId.toCreature();
	picture = std::make_shared<CCreaturePic>(picture->pos.x - pos.x, picture->pos.y - pos.y, creatureOnTheCard);
	creatureClickArea = std::make_shared<CCreatureClickArea>(Point(picture->pos.x - pos.x, picture->pos.y - pos.y), picture, creatureOnTheCard);
	parent->updateAllSliders();
	cost->set(creatureOnTheCard->getFullRecruitCost() * slider->getValue());
	updateAccessibilityInfo();
	
	// Update slider accessibility info with new creature name
	if(slider)
	{
		slider->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("slider")
			.withName(creatureOnTheCard->getNamePluralTranslated() + " quantity")
			.withDescription("Adjust recruitment amount from 0 to " + std::to_string(maxAmount))
			.withValue(std::to_string(slider->getValue()))
			.withTabOrder(parent->getCardsCount() * 10 + 1)
		);
	}
}

void CreaturePurchaseCard::initAmountInfo()
{
	availableAmount = std::make_shared<CLabel>(pos.x + 25, pos.y + 146, FONT_SMALL, ETextAlignment::CENTER, Colors::YELLOW);
	purchaseAmount = std::make_shared<CLabel>(pos.x + 76, pos.y + 146, FONT_SMALL, ETextAlignment::CENTER, Colors::WHITE);
	updateAmountInfo(0);
}

void CreaturePurchaseCard::updateAmountInfo(int value)
{
	availableAmount->setText(std::to_string(maxAmount-value));
	purchaseAmount->setText(std::to_string(value));
}

void CreaturePurchaseCard::initSlider()
{
	slider = std::make_shared<CSlider>(Point(pos.x, pos.y + 158), 102, std::bind(&CreaturePurchaseCard::sliderMoved, this, _1), 0, maxAmount, 0, Orientation::HORIZONTAL);
	slider->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("slider")
		.withName(creatureOnTheCard->getNamePluralTranslated() + " quantity")
		.withDescription("Adjust recruitment amount from 0 to " + std::to_string(maxAmount))
		.withValue(std::to_string(0))
		.withTabOrder(parent->getCardsCount() * 10 + 1)
	);
}

void CreaturePurchaseCard::initCostBox()
{
	cost = std::make_shared<CreatureCostBox>(Rect(pos.x+2, pos.y + 194, 97, 74), "");
	cost->createItems(creatureOnTheCard->getFullRecruitCost());
}

void CreaturePurchaseCard::sliderMoved(int to)
{
	updateAmountInfo(to);
	cost->set(creatureOnTheCard->getFullRecruitCost() * to);
	parent->updateAllSliders();
	updateAccessibilityInfo();
	
	// Announce value change if this card has focus
	if(focusState)
	{
		AccessibilityManager::getInstance().announce("Selected " + std::to_string(to) + " " + creatureOnTheCard->getNamePluralTranslated());
	}
}

CreaturePurchaseCard::CreaturePurchaseCard(const std::vector<CreatureID> & creaturesID, Point position, int creaturesMaxAmount, QuickRecruitmentWindow * parents)
	: upgradesID(creaturesID),
	parent(parents),
	maxAmount(creaturesMaxAmount)
{
	creatureOnTheCard = upgradesID.back().toCreature();
	moveTo(Point(position.x, position.y));
	initView();
}

void CreaturePurchaseCard::initView()
{
	picture = std::make_shared<CCreaturePic>(pos.x, pos.y, creatureOnTheCard);
	background = std::make_shared<CPicture>(ImagePath::builtin("QuickRecruitmentWindow/CreaturePurchaseCard.png"), pos.x-4, pos.y-50);
	creatureClickArea = std::make_shared<CCreatureClickArea>(Point(pos.x, pos.y), picture, creatureOnTheCard);

	initAmountInfo();
	initSlider();
	initButtons(); // order important! buttons need slider!
	initCostBox();
	updateAccessibilityInfo();
}

CreaturePurchaseCard::CCreatureClickArea::CCreatureClickArea(const Point & position, const std::shared_ptr<CCreaturePic> creaturePic, const CCreature * creatureOnTheCard)
	: CIntObject(SHOW_POPUP),
	creatureOnTheCard(creatureOnTheCard)
{
	pos.x += position.x;
	pos.y += position.y;
	pos.w = CREATURE_WIDTH;
	pos.h = CREATURE_HEIGHT;
}

void CreaturePurchaseCard::CCreatureClickArea::showPopupWindow(const Point & cursorPosition)
{
	ENGINE->windows().createAndPushWindow<CStackWindow>(creatureOnTheCard, true);
}

void CreaturePurchaseCard::keyPressed(EShortcut key)
{
	switch(key)
	{
		case EShortcut::MOVE_UP:
			slider->scrollBy(1);
			break;
			
		case EShortcut::MOVE_DOWN:
			slider->scrollBy(-1);
			break;
			
		case EShortcut::MOVE_PAGE_UP:
			slider->scrollBy(10);
			break;
			
		case EShortcut::MOVE_PAGE_DOWN:
			slider->scrollBy(-10);
			break;
			
		case EShortcut::MOVE_FIRST:
			slider->scrollToMin();
			break;
			
		case EShortcut::MOVE_LAST:
			slider->scrollToMax();
			break;
			
		case EShortcut::GLOBAL_ACCEPT:
			// Show creature info window
			if(creatureClickArea)
				creatureClickArea->showPopupWindow(Point());
			break;
			
		case EShortcut::RECRUITMENT_MAX:
			if(maxButton)
				maxButton->clickPressed(Point());
			break;
			
		case EShortcut::RECRUITMENT_MIN:
			if(minButton)
				minButton->clickPressed(Point());
			break;
			
		case EShortcut::RECRUITMENT_SWITCH_LEVEL:
			if(creatureSwitcher && creatureSwitcher->isActive())
				switchCreatureLevel();
			break;
	}
}

void CreaturePurchaseCard::onFocusGained()
{
	// Update accessibility info with current state
	updateAccessibilityInfo();
	
	// Announce creature details
	if(AccessibilityManager::getInstance().isScreenReaderEnabled())
	{
		std::string announcement = creatureOnTheCard->getNamePluralTranslated() + 
			", Level " + std::to_string(creatureOnTheCard->getLevel()) +
			". Available: " + std::to_string(maxAmount - slider->getValue()) +
			", Selected: " + std::to_string(slider->getValue()) +
			". Cost per unit: " + creatureOnTheCard->getFullRecruitCost().toString();
		
		AccessibilityManager::getInstance().announce(announcement);
	}
}

void CreaturePurchaseCard::onFocusLost()
{
	// Visual feedback can be handled here if needed
}

// isFocusable() is already defined inline in the header

int CreaturePurchaseCard::getCurrentAmount() const
{
	return slider->getValue();
}

void CreaturePurchaseCard::setCardFocus()
{
	FocusManager::getInstance().setFocus(this);
}

void CreaturePurchaseCard::updateAccessibilityInfo()
{
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("group")
		.withName(creatureOnTheCard->getNamePluralTranslated() + " recruitment card")
		.withDescription("Level " + std::to_string(creatureOnTheCard->getLevel()) + " creature")
		.withValue("Selected: " + std::to_string(slider->getValue()) + " of " + std::to_string(maxAmount))
		.withState(slider->getValue() > 0 ? "has selection" : "no selection")
	);
}
