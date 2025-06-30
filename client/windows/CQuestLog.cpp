/*
 * CQuestLog.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "CQuestLog.h"

#include "../CPlayerInterface.h"

#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../CPlayerInterface.h"
#include "../gui/Shortcut.h"
#include "../gui/AccessibilityManager.h"
#include "../widgets/Buttons.h"
#include "../widgets/CComponent.h"
#include "../widgets/Slider.h"
#include "../adventureMap/AdventureMapInterface.h"
#include "../adventureMap/CMinimap.h"
#include "../render/Canvas.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/gameState/QuestInfo.h"
#include "../../lib/mapObjects/CQuest.h"
#include "../../lib/texts/CGeneralTextHandler.h"

VCMI_LIB_NAMESPACE_BEGIN

struct QuestInfo;

VCMI_LIB_NAMESPACE_END

class CAdvmapInterface;

void CQuestLabel::clickPressed(const Point & cursorPosition)
{
	callback();
	// Announce the selected quest when clicked
	if(AccessibilityManager::getInstance().isScreenReaderEnabled())
	{
		AccessibilityManager::getInstance().announceElement(this);
	}
}

void CQuestLabel::showAll(Canvas & to)
{
	CMultiLineLabel::showAll (to);
}

void CQuestLabel::keyPressed(EShortcut key)
{
	if(key == EShortcut::GLOBAL_ACCEPT)
	{
		clickPressed(Point());
	}
}

CQuestIcon::CQuestIcon (const AnimationPath &defname, int index, int x, int y) :
	CAnimImage(defname, index, 0, x, y)
{
	addUsedEvents(LCLICK | KEYBOARD);
}

void CQuestIcon::clickPressed(const Point & cursorPosition)
{
	callback();
}

void CQuestIcon::showAll(Canvas & to)
{
	CanvasClipRectGuard guard(to, parent->pos);
	CAnimImage::showAll(to);
}

CQuestMinimap::CQuestMinimap(const Rect & position)
	: CMinimap(position),
	currentQuest(nullptr)
{
}

void CQuestMinimap::addQuestMarks (const QuestInfo * q)
{
	OBJECT_CONSTRUCTION;
	icons.clear();

	int3 tile = q->getPosition(GAME->interface()->cb.get());

	Point offset = tileToPixels(tile);

	onMapViewMoved(Rect(), tile.z);

	auto pic = std::make_shared<CQuestIcon>(AnimationPath::builtin("VwSymbol.def"), 3, offset.x, offset.y);

	pic->moveBy (Point ( -pic->pos.w/2, -pic->pos.h/2));
	pic->callback = std::bind (&CQuestMinimap::iconClicked, this);
	pic->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("marker")
		.withName("Quest location")
		.withDescription("Click to center map on quest location"));

	icons.push_back(pic);
}

void CQuestMinimap::update()
{
	CMinimap::update();
	if(currentQuest)
		addQuestMarks(currentQuest);
}

void CQuestMinimap::iconClicked()
{
	if(currentQuest->obj.hasValue())
	{
		adventureInt->centerOnTile(currentQuest->getObject(GAME->interface()->cb.get())->visitablePos());
		
		// Announce action to screen reader
		if(AccessibilityManager::getInstance().isScreenReaderEnabled())
		{
			AccessibilityManager::getInstance().announce("Centered map on quest location");
		}
	}
	//moveAdvMapSelection();
}

void CQuestMinimap::showAll(Canvas & to)
{
	CIntObject::showAll(to); // blitting IntObject directly to hide radar
//	for (auto pic : icons)
//		pic->showAll(to);
}

CQuestLog::CQuestLog (const std::vector<QuestInfo> & Quests)
	: CWindowObject(PLAYER_COLORED | BORDERED, ImagePath::builtin("questDialog")),
	questIndex(0),
	currentQuest(nullptr),
	hideComplete(false),
	quests(Quests)
{
	OBJECT_CONSTRUCTION;
	
	// Enable keyboard events
	addUsedEvents(KEYBOARD);
	
	// Set accessibility info for the window
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("window")
		.withName("Quest Log")
		.withDescription("View and track active quests"));

	minimap = std::make_shared<CQuestMinimap>(Rect(12, 12, 169, 169));
	minimap->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("map")
		.withName("Quest Map")
		.withDescription("Shows quest location on map")
		.withTabOrder(20));
	
	// TextBox have it's own 4 pixel padding from top at least for English. To achieve 10px from both left and top only add 6px margin
	description = std::make_shared<CTextBox>("", Rect(205, 18, 385, DESCRIPTION_HEIGHT_MAX), CSlider::BROWN, FONT_MEDIUM, ETextAlignment::TOPLEFT, Colors::WHITE);
	description->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("text")
		.withName("Quest Description")
		.withDescription("Details about the selected quest")
		.withTabOrder(30));
	
	ok = std::make_shared<CButton>(Point(539, 398), AnimationPath::builtin("IOKAY.DEF"), LIBRARY->generaltexth->zelp[445], std::bind(&CQuestLog::close, this), EShortcut::GLOBAL_RETURN);
	ok->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Close")
		.withDescription("Close quest log window")
		.withTabOrder(50));
	
	// Both button and label are shifted to -2px by x and y to not make them actually look like they're on same line with quests list and ok button
	hideCompleteButton = std::make_shared<CToggleButton>(Point(10, 396), AnimationPath::builtin("sysopchk.def"), CButton::tooltipLocalized("vcmi.questLog.hideComplete"), std::bind(&CQuestLog::toggleComplete, this, _1));
	hideCompleteButton->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("checkbox")
		.withName("Hide completed quests")
		.withDescription("Toggle visibility of completed quests")
		.withTabOrder(40));
	
	hideCompleteLabel = std::make_shared<CLabel>(46, 398, FONT_MEDIUM, ETextAlignment::TOPLEFT, Colors::WHITE, LIBRARY->generaltexth->translate("vcmi.questLog.hideComplete.hover"));
	hideCompleteLabel->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("label")
		.withName("Hide completed quests label")
		.withDescription(LIBRARY->generaltexth->translate("vcmi.questLog.hideComplete.hover")));
	
	slider = std::make_shared<CSlider>(Point(166, 195), 191, std::bind(&CQuestLog::sliderMoved, this, _1), QUEST_COUNT, 0, 0, Orientation::VERTICAL, CSlider::BROWN);
	slider->setPanningStep(32);
	slider->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("scrollbar")
		.withName("Quest list scrollbar")
		.withDescription("Scroll through the list of quests")
		.withTabOrder(35));

	recreateLabelList();
	recreateQuestList(0);
	
	// Set initial focus to the first quest if available
	if(!labels.empty() && !labels[0]->isDisabled())
	{
		labels[0]->setFocus(true);
	}
	
	// Announce window opening to screen reader
	if(AccessibilityManager::getInstance().isScreenReaderEnabled())
	{
		int activeQuests = 0;
		for(const auto& quest : quests)
		{
			if(!quest.getQuest(GAME->interface()->cb.get())->isCompleted)
				activeQuests++;
		}
		std::string announcement = "Quest Log opened. " + std::to_string(activeQuests) + " active quests";
		AccessibilityManager::getInstance().announce(announcement);
	}
}

void CQuestLog::recreateLabelList()
{
	OBJECT_CONSTRUCTION;
	labels.clear();

	bool completeMissing = true;
	int currentLabel = 0;
	for (int i = 0; i < quests.size(); ++i)
	{
		auto questPtr = quests[i].getQuest(GAME->interface()->cb.get());
		auto questObject = quests[i].getObject(GAME->interface()->cb.get());

		// Quests without mision don't have text for them and can't be displayed
		if (quests[i].getQuest(GAME->interface()->cb.get())->mission == Rewardable::Limiter{})
			continue;

		if (questPtr->isCompleted)
		{
			completeMissing = false;
			if (hideComplete)
				continue;
		}

		MetaString text;
		questPtr->getRolloverText(GAME->interface()->cb.get(), text, false);
		if (quests[i].obj.hasValue())
		{
			if (auto seersHut = dynamic_cast<const CGSeerHut *>(questObject))
			{
				MetaString toSeer;
				toSeer.appendRawString(LIBRARY->generaltexth->allTexts[347]);
				toSeer.replaceRawString(seersHut->seerName);
				text.replaceRawString(toSeer.toString());
			}
			else
				text.replaceRawString(questObject->getObjectName()); //get name of the object
		}
		auto label = std::make_shared<CQuestLabel>(Rect(13, 195, 149,31), FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, text.toString());
		label->disable();
		
		// Set accessibility info for each quest label
		std::string questStatus = questPtr->isCompleted ? "completed" : "active";
		label->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("listitem")
			.withName(text.toString())
			.withDescription("Quest: " + text.toString())
			.withState(questStatus)
			.withTabOrder(currentLabel + 1));

		label->callback = std::bind(&CQuestLog::selectQuest, this, i, currentLabel);
		labels.push_back(label);

		// Select latest active quest
		if(!questPtr->isCompleted)
			selectQuest(i, currentLabel);

		currentLabel = static_cast<int>(labels.size());
	}

	if (completeMissing) // We can't use block(completeMissing) because if false button state reset to NORMAL
		hideCompleteButton->block(true);

	slider->setAmount(currentLabel);
	if (currentLabel > QUEST_COUNT)
	{
		slider->block(false);
		slider->scrollToMax();
	}
	else
	{
		slider->block(true);
		slider->scrollToMin();
	}
}

void CQuestLog::showAll(Canvas & to)
{
	CWindowObject::showAll(to);
	if(questIndex >= 0 && questIndex < labels.size())
	{
		//TODO: use child object to selection rect
		Rect rect = Rect::createAround(labels[questIndex]->pos, 1);
		rect.x -= 2; // Adjustment needed as we want selection box on top of border in graphics
		rect.w += 2;
		to.drawBorder(rect, Colors::METALLIC_GOLD);
		
		// Draw focus indicator for keyboard navigation
		if(labels[questIndex]->hasFocus())
		{
			Rect focusRect = Rect::createAround(labels[questIndex]->pos, 3);
			focusRect.x -= 4;
			focusRect.w += 4;
			to.drawBorder(focusRect, Colors::BRIGHT_YELLOW);
		}
	}
}

void CQuestLog::recreateQuestList (int newpos)
{
	for (int i = 0; i < labels.size(); ++i)
	{
		labels[i]->pos = Rect (pos.x + 14, pos.y + 195 + (i-newpos) * 32, 151, 31);
		if (i >= newpos && i < newpos + QUEST_COUNT)
			labels[i]->enable();
		else
			labels[i]->disable();
	}
	minimap->update();
}

void CQuestLog::selectQuest(int which, int labelId)
{
	questIndex = labelId;
	currentQuest = &quests[which];
	minimap->currentQuest = currentQuest;

	MetaString text;
	std::vector<Component> components;
	currentQuest->getQuest(GAME->interface()->cb.get())->getVisitText(GAME->interface()->cb.get(), text, components, true);
	if(description->slider)
		description->slider->scrollToMin(); // scroll text to start position
	description->setText(text.toString()); //TODO: use special log entry text
	
	// Update description accessibility info with current quest details
	if(description)
	{
		description->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("text")
			.withName("Quest Description")
			.withDescription(text.toString())
			.withTabOrder(30));
	}
	
	// Announce the selected quest to screen reader
	if(AccessibilityManager::getInstance().isScreenReaderEnabled() && labelId < labels.size())
	{
		AccessibilityManager::getInstance().announceElement(labels[labelId].get());
	}

	componentsBox.reset();

	int componentsSize = static_cast<int>(components.size());
	int descriptionHeight = DESCRIPTION_HEIGHT_MAX;
	if(componentsSize)
	{
		CComponent::ESize imageSize = CComponent::large;
		if (componentsSize > 4)
		{
			imageSize = CComponent::small; // Only small icons can be used for resources as 4+ icons take too much space
			descriptionHeight -= 155;
		}
		else
			descriptionHeight -= 130;
		/*switch (currentQuest->quest->missionType)
		{
			case CQuest::MISSION_ARMY:
			{
				if (componentsSize > 4)
					descriptionHeight -= 195;
				else
					descriptionHeight -= 100;

				break;
			}
			case CQuest::MISSION_ART:
			{
				if (componentsSize > 4)
					descriptionHeight -= 190;
				else
					descriptionHeight -= 90;

				break;
			}
			case CQuest::MISSION_PRIMARY_STAT:
			case CQuest::MISSION_RESOURCES:
			{
				if (componentsSize > 4)
				{
					imageSize = CComponent::small; // Only small icons can be used for resources as 4+ icons take too much space
					descriptionHeight -= 140;
				}
				else
					descriptionHeight -= 125;

				break;
			}
			default:
				descriptionHeight -= 115;
				break;
		}*/

		OBJECT_CONSTRUCTION;

		std::vector<std::shared_ptr<CComponent>> comps;
		for(auto & component : components)
		{
			auto c = std::make_shared<CComponent>(component, imageSize);
			comps.push_back(c);
		}

		componentsBox = std::make_shared<CComponentBox>(comps, Rect(202, 20+descriptionHeight+15, 391, DESCRIPTION_HEIGHT_MAX-(20+descriptionHeight)));
	}
	description->resize(Point(385, descriptionHeight));

	minimap->update();
	redraw();
}

void CQuestLog::sliderMoved(int newpos)
{
	recreateQuestList(newpos); //move components
	redraw();
}

void CQuestLog::toggleComplete(bool on)
{
	hideComplete = on;
	recreateLabelList();
	recreateQuestList(0);
	redraw();
	
	// Update accessibility state of the checkbox
	if(hideCompleteButton)
	{
		hideCompleteButton->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("checkbox")
			.withName("Hide completed quests")
			.withDescription("Toggle visibility of completed quests")
			.withState(on ? "checked" : "unchecked")
			.withTabOrder(40));
	}
	
	// Announce state change to screen reader
	if(AccessibilityManager::getInstance().isScreenReaderEnabled())
	{
		std::string announcement = on ? "Hiding completed quests" : "Showing completed quests";
		AccessibilityManager::getInstance().announce(announcement);
	}
}

void CQuestLog::keyPressed(EShortcut key)
{
	switch(key)
	{
	case EShortcut::MOVE_UP:
		selectPreviousQuest();
		break;
	case EShortcut::MOVE_DOWN:
		selectNextQuest();
		break;
	case EShortcut::MOVE_PAGE_UP:
		// Page up - move up by QUEST_COUNT items
		for(int i = 0; i < QUEST_COUNT && questIndex > 0; i++)
			selectPreviousQuest();
		break;
	case EShortcut::MOVE_PAGE_DOWN:
		// Page down - move down by QUEST_COUNT items
		for(int i = 0; i < QUEST_COUNT && questIndex < labels.size() - 1; i++)
			selectNextQuest();
		break;
	case EShortcut::MOVE_FIRST:
		// Home - select first quest
		if(!labels.empty())
		{
			int firstIndex = 0;
			for(; firstIndex < labels.size() && labels[firstIndex]->isDisabled(); firstIndex++);
			if(firstIndex < labels.size())
			{
				labels[questIndex]->setFocus(false);
				questIndex = firstIndex;
				labels[questIndex]->setFocus(true);
				labels[questIndex]->callback();
				if(slider)
					slider->scrollToMin();
			}
		}
		break;
	case EShortcut::MOVE_LAST:
		// End - select last quest
		if(!labels.empty())
		{
			int lastIndex = labels.size() - 1;
			for(; lastIndex >= 0 && labels[lastIndex]->isDisabled(); lastIndex--);
			if(lastIndex >= 0)
			{
				labels[questIndex]->setFocus(false);
				questIndex = lastIndex;
				labels[questIndex]->setFocus(true);
				labels[questIndex]->callback();
				if(slider)
					slider->scrollToMax();
			}
		}
		break;
	default:
		CWindowObject::keyPressed(key);
		break;
	}
}

void CQuestLog::selectPreviousQuest()
{
	if(questIndex > 0)
	{
		// Find previous enabled quest
		int prevIndex = questIndex - 1;
		while(prevIndex >= 0 && labels[prevIndex]->isDisabled())
			prevIndex--;
		
		if(prevIndex >= 0)
		{
			labels[questIndex]->setFocus(false);
			questIndex = prevIndex;
			labels[questIndex]->setFocus(true);
			labels[questIndex]->callback();
			
			// Adjust slider if needed
			if(slider && questIndex < slider->getValue())
			{
				slider->scrollBy(-1);
			}
		}
	}
}

void CQuestLog::selectNextQuest()
{
	if(questIndex < labels.size() - 1)
	{
		// Find next enabled quest
		int nextIndex = questIndex + 1;
		while(nextIndex < labels.size() && labels[nextIndex]->isDisabled())
			nextIndex++;
		
		if(nextIndex < labels.size())
		{
			labels[questIndex]->setFocus(false);
			questIndex = nextIndex;
			labels[questIndex]->setFocus(true);
			labels[questIndex]->callback();
			
			// Adjust slider if needed
			if(slider && questIndex >= slider->getValue() + QUEST_COUNT)
			{
				slider->scrollBy(1);
			}
		}
	}
}
