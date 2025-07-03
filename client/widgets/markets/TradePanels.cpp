/*
 * TradePanels.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "TradePanels.h"

#include "../../GameEngine.h"
#include "../../GameInstance.h"
#include "../../render/Canvas.h"
#include "../../widgets/TextControls.h"
#include "../../windows/InfoWindows.h"
#include "../../gui/AccessibilityManager.h"
#include "../../gui/Shortcut.h"

#include "../../CPlayerInterface.h"

#include "../../../lib/GameLibrary.h"
#include "../../../lib/callback/CCallback.h"
#include "../../../lib/entities/artifact/CArtHandler.h"
#include "../../../lib/texts/CGeneralTextHandler.h"
#include "../../../lib/mapObjects/CGHeroInstance.h"

CTradeableItem::CTradeableItem(const Rect & area, EType Type, int32_t ID, int32_t serial)
	: SelectableSlot(area, Point(1, 1))
	, type(EType(-1)) // set to invalid, will be corrected in setType
	, id(ID)
	, serial(serial)
{
	OBJECT_CONSTRUCTION;

	addUsedEvents(LCLICK);
	addUsedEvents(HOVER);
	addUsedEvents(SHOW_POPUP);
	addUsedEvents(KEYBOARD);
	
	subtitle = std::make_shared<CLabel>(0, 0, FONT_SMALL, ETextAlignment::CENTER, Colors::WHITE);
	setType(Type);

	this->pos.w = area.w;
	this->pos.h = area.h;
	
	// Set up accessibility info for the tradeable item
	updateAccessibilityInfo();
}

void CTradeableItem::setType(EType newType)
{
	if(type != newType)
	{
		OBJECT_CONSTRUCTION;
		type = newType;

		if(getIndex() < 0)
		{
			image = std::make_shared<CAnimImage>(getFilename(), 0);
			image->disable();
		}
		else
		{
			image = std::make_shared<CAnimImage>(getFilename(), getIndex());
		}

		switch(type)
		{
		case EType::RESOURCE:
			subtitle->moveTo(pos.topLeft() + Point(35, 55));
			image->moveTo(pos.topLeft() + Point(19, 8));
			break;
		case EType::CREATURE:
			subtitle->moveTo(pos.topLeft() + Point(30, 77));
			break;
		case EType::PLAYER:
			subtitle->moveTo(pos.topLeft() + Point(31, 76));
			break;
		case EType::ARTIFACT:
			subtitle->moveTo(pos.topLeft() + Point(21, 55));
			break;
		case EType::ARTIFACT_TYPE:
			subtitle->moveTo(pos.topLeft() + Point(35, 57));
			image->moveTo(pos.topLeft() + Point(13, 0));
			break;
		}
		
		// Update accessibility info when type changes
		updateAccessibilityInfo();
	}
}

void CTradeableItem::setID(int32_t newID)
{
	if(id != newID)
	{
		id = newID;
		if(image)
		{
			const auto index = getIndex();
			if(index < 0)
				image->disable();
			else
			{
				image->enable();
				image->setFrame(index);
			}
		}
		
		// Update accessibility info when ID changes
		updateAccessibilityInfo();
	}
}

void CTradeableItem::clear()
{
	setID(-1);
	image->setFrame(0);
	image->disable();
	subtitle->clear();
}

AnimationPath CTradeableItem::getFilename()
{
	switch(type)
	{
	case EType::RESOURCE:
		return AnimationPath::builtin("RESOURCE");
	case EType::PLAYER:
		return AnimationPath::builtin("CREST58");
	case EType::ARTIFACT_TYPE:
	case EType::ARTIFACT:
		return AnimationPath::builtin("artifact");
	case EType::CREATURE:
		return AnimationPath::builtin("TWCRPORT");
	default:
		return {};
	}
}

int CTradeableItem::getIndex()
{
	if(id < 0)
		return -1;

	switch(type)
	{
	case EType::RESOURCE:
	case EType::PLAYER:
		return id;
	case EType::ARTIFACT_TYPE:
	case EType::ARTIFACT:
		return LIBRARY->artifacts()->getByIndex(id)->getIconIndex();
	case EType::CREATURE:
		return LIBRARY->creatures()->getByIndex(id)->getIconIndex();
	default:
		return -1;
	}
}

void CTradeableItem::clickPressed(const Point & cursorPosition)
{
	if(clickPressedCallback)
	{
		clickPressedCallback(shared_from_this());
		// Update accessibility info after selection changes
		updateAccessibilityInfo();
	}
}

void CTradeableItem::hover(bool on)
{
	if(!on || id == -1)
	{
		ENGINE->statusbar()->clear();
		return;
	}

	switch(type)
	{
	case EType::CREATURE:
		ENGINE->statusbar()->write(boost::str(boost::format(LIBRARY->generaltexth->allTexts[481]) % LIBRARY->creh->objects[id]->getNamePluralTranslated()));
		break;
	case EType::ARTIFACT_TYPE:
	case EType::ARTIFACT:
		if(id < 0)
			ENGINE->statusbar()->write(LIBRARY->generaltexth->zelp[582].first);
		else
			ENGINE->statusbar()->write(LIBRARY->artifacts()->getByIndex(id)->getNameTranslated());
		break;
	case EType::RESOURCE:
		ENGINE->statusbar()->write(LIBRARY->generaltexth->restypes[id]);
		break;
	case EType::PLAYER:
		ENGINE->statusbar()->write(LIBRARY->generaltexth->capColors[id]);
		break;
	}
}

void CTradeableItem::showPopupWindow(const Point & cursorPosition)
{
	switch(type)
	{
	case EType::CREATURE:
		break;
	case EType::ARTIFACT_TYPE:
	case EType::ARTIFACT:
		if (id >= 0)
			CRClickPopup::createAndPush(LIBRARY->artifacts()->getByIndex(id)->getDescriptionTranslated());
		break;
	}
}

void CTradeableItem::updateAccessibilityInfo()
{
	UIAccessibilityInfo accessInfo;
	accessInfo.role = "Tradeable Slot";
	accessInfo.isAccessible = (id >= 0);
	
	if(id < 0)
	{
		accessInfo.name = "Empty slot";
		accessInfo.state = "empty";
	}
	else
	{
		// Set name based on type
		switch(type)
		{
		case EType::RESOURCE:
			accessInfo.name = LIBRARY->generaltexth->restypes[id];
			accessInfo.description = "Resource for trading";
			break;
		case EType::CREATURE:
			accessInfo.name = LIBRARY->creh->objects[id]->getNamePluralTranslated();
			accessInfo.description = "Creature for trading";
			break;
		case EType::ARTIFACT:
		case EType::ARTIFACT_TYPE:
			accessInfo.name = LIBRARY->artifacts()->getByIndex(id)->getNameTranslated();
			accessInfo.description = LIBRARY->artifacts()->getByIndex(id)->getDescriptionTranslated();
			break;
		case EType::PLAYER:
			accessInfo.name = LIBRARY->generaltexth->capColors[id];
			accessInfo.description = "Player for resource transfer";
			break;
		}
		
		// Add quantity info if available
		if(!subtitle->getText().empty())
		{
			accessInfo.value = "Quantity: " + subtitle->getText();
		}
		
		// Add selection state
		if(isSelected())
		{
			accessInfo.state = "selected";
		}
		else
		{
			accessInfo.state = "available";
		}
	}
	
	setAccessibilityInfo(accessInfo);
}

void CTradeableItem::keyPressed(EShortcut key)
{
	if(key == EShortcut::SELECT_INDEX_1 || key == EShortcut::GLOBAL_ACCEPT)
	{
		clickPressed(pos.center());
	}
}

void CTradeableItem::onFocusGained()
{
	// Announce the item when focused
	updateAccessibilityInfo();
	AccessibilityManager::getInstance().announceElement(this);
	
	// Visual feedback
	hover(true);
}

void CTradeableItem::onFocusLost()
{
	// Remove visual feedback
	hover(false);
}

void TradePanelBase::update()
{
	if(deleteSlotsCheck)
		slots.erase(std::remove_if(slots.begin(), slots.end(), deleteSlotsCheck), slots.end());

	if(updateSlotsCallback)
		updateSlotsCallback();
}

void TradePanelBase::deselect()
{
	for(const auto & slot : slots)
		slot->selectSlot(false);
}

void TradePanelBase::clearSubtitles()
{
	for(const auto & slot : slots)
		slot->subtitle->clear();
}

void TradePanelBase::updateOffer(CTradeableItem & slot, int cost, int qty)
{
	std::string subtitle = std::to_string(qty);
	if(cost != 1)
	{
		subtitle.append("/");
		subtitle.append(std::to_string(cost));
	}
	slot.subtitle->setText(subtitle);
	
	// Update accessibility info with new quantity
	slot.updateAccessibilityInfo();
}

void TradePanelBase::setShowcaseSubtitle(const std::string & text)
{
	showcaseSlot->subtitle->setText(text);
}

int32_t TradePanelBase::getHighlightedItemId() const
{
	if(highlightedSlot)
		return highlightedSlot->id;
	else
		return -1;
}

void TradePanelBase::onSlotClickPressed(const std::shared_ptr<CTradeableItem> & newSlot)
{
	assert(vstd::contains(slots, newSlot));
	if(newSlot == highlightedSlot)
		return;

	if(highlightedSlot)
		highlightedSlot->selectSlot(false);
	highlightedSlot = newSlot;
	newSlot->selectSlot(true);
	
	// Announce selection
	if(newSlot->getAccessibilityInfo())
	{
		std::string announcement = "Selected: " + newSlot->getAccessibilityInfo()->name;
		AccessibilityManager::getInstance().announce(announcement, true);
	}
}

bool TradePanelBase::isHighlighted() const
{
	return highlightedSlot != nullptr;
}

bool TradePanelBase::captureThisKey(EShortcut key)
{
	// Don't capture Tab navigation keys - let the focus system handle them
	if(key == EShortcut::GLOBAL_MOVE_FOCUS || key == EShortcut::GLOBAL_MOVE_FOCUS_PREV)
		return false;
		
	// Only capture keys when this panel has focus
	if (!hasFocus())
		return false;
		
	// Capture arrow keys and accept key
	return key == EShortcut::MOVE_UP || 
	       key == EShortcut::MOVE_DOWN ||
	       key == EShortcut::MOVE_LEFT ||
	       key == EShortcut::MOVE_RIGHT ||
	       key == EShortcut::MOVE_FIRST ||
	       key == EShortcut::MOVE_LAST ||
	       key == EShortcut::GLOBAL_ACCEPT;
}

void TradePanelBase::keyPressed(EShortcut key)
{
	// Only handle keys if this panel has focus
	if(!hasFocus())
	{
		CIntObject::keyPressed(key);
		return;
	}
	
	// Only handle arrow keys and Enter when focused - let Tab pass through to parent
	switch(key)
	{
		case EShortcut::MOVE_UP:
		case EShortcut::MOVE_DOWN:
		case EShortcut::MOVE_LEFT:
		case EShortcut::MOVE_RIGHT:
		case EShortcut::MOVE_FIRST:
		case EShortcut::MOVE_LAST:
			handleArrowKeyNavigation(key);
			break;
		case EShortcut::GLOBAL_ACCEPT:
			// Enter key selects the focused item
			if(focusedSlotIndex >= 0 && focusedSlotIndex < slots.size() && slots[focusedSlotIndex]->id >= 0)
			{
				// Trigger the actual click callback, not just selection
				slots[focusedSlotIndex]->clickPressed(slots[focusedSlotIndex]->pos.center());
			}
			break;
		default:
			// Let other keys (including Tab) pass through
			CIntObject::keyPressed(key);
			break;
	}
}

void TradePanelBase::handleArrowKeyNavigation(EShortcut key)
{
	if(slots.empty())
		return;
	
	// Find the number of columns based on the panel type
	int columns = 3; // Default for most panels
	if(dynamic_cast<const ArtifactsAltarPanel*>(this))
		columns = 5; // Special case for artifact altar
		
	int currentIndex = focusedSlotIndex;
	
	// If no slot is focused, start with the first valid slot
	if(currentIndex < 0)
	{
		for(int i = 0; i < slots.size(); i++)
		{
			if(slots[i]->id >= 0)
			{
				moveFocusToSlot(i);
				return;
			}
		}
		return;
	}
	
	int newIndex = currentIndex;
	
	switch(key)
	{
		case EShortcut::MOVE_UP:
			newIndex = currentIndex - columns;
			break;
		case EShortcut::MOVE_DOWN:
			newIndex = currentIndex + columns;
			break;
		case EShortcut::MOVE_LEFT:
			newIndex = currentIndex - 1;
			break;
		case EShortcut::MOVE_RIGHT:
			newIndex = currentIndex + 1;
			break;
		case EShortcut::MOVE_FIRST:
			newIndex = 0;
			break;
		case EShortcut::MOVE_LAST:
			newIndex = slots.size() - 1;
			break;
	}
	
	// Wrap around horizontally
	if(key == EShortcut::MOVE_LEFT && currentIndex % columns == 0)
		newIndex = currentIndex + columns - 1;
	else if(key == EShortcut::MOVE_RIGHT && (currentIndex + 1) % columns == 0)
		newIndex = currentIndex - columns + 1;
	
	// Clamp to valid range
	if(newIndex >= 0 && newIndex < slots.size())
	{
		moveFocusToSlot(newIndex);
	}
}

void TradePanelBase::moveFocusToSlot(int newIndex)
{
	if(newIndex < 0 || newIndex >= slots.size())
		return;
		
	// Remove focus from current slot
	if(focusedSlotIndex >= 0 && focusedSlotIndex < slots.size())
	{
		slots[focusedSlotIndex]->setFocus(false);
	}
	
	// Set focus to new slot
	focusedSlotIndex = newIndex;
	if(slots[focusedSlotIndex]->id >= 0) // Only focus valid slots
	{
		slots[focusedSlotIndex]->setFocus(true);
		
		// Announce the focused slot
		AccessibilityManager::getInstance().announceElement(slots[focusedSlotIndex].get());
	}
	else
	{
		// Skip empty slots and find next valid one
		int direction = (newIndex > focusedSlotIndex) ? 1 : -1;
		for(int i = newIndex; i >= 0 && i < slots.size(); i += direction)
		{
			if(slots[i]->id >= 0)
			{
				focusedSlotIndex = i;
				slots[focusedSlotIndex]->setFocus(true);
				AccessibilityManager::getInstance().announceElement(slots[focusedSlotIndex].get());
				break;
			}
		}
	}
}

void TradePanelBase::setupKeyboardNavigation()
{
	// Enable keyboard events for the panel
	addUsedEvents(KEYBOARD);
	
	// Don't set tab order on individual items - the panel itself is focusable
}

void TradePanelBase::setTabOrder(int order)
{
	auto currentInfo = getAccessibilityInfo();
	if(currentInfo)
	{
		UIAccessibilityInfo newInfo = *currentInfo;
		newInfo.tabOrder = order;
		setAccessibilityInfo(newInfo);
	}
	else
	{
		setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("panel")
			.withName("Trade panel")
			.withTabOrder(order));
	}
}

ResourcesPanel::ResourcesPanel(const CTradeableItem::ClickPressedFunctor & clickPressedCallback,
	const UpdateSlotsFunctor & updateSubtitles)
{
	assert(resourcesForTrade.size() == slotsPos.size());
	OBJECT_CONSTRUCTION;

	for(const auto & res : resourcesForTrade)
	{
		auto slot = slots.emplace_back(std::make_shared<CTradeableItem>(Rect(slotsPos[res.num], slotDimension), EType::RESOURCE, res.num, res.num));
		slot->clickPressedCallback = clickPressedCallback;
		slot->setSelectionWidth(selectionWidth);
	}
	updateSlotsCallback = updateSubtitles;
	showcaseSlot = std::make_shared<CTradeableItem>(Rect(selectedPos, slotDimension), EType::RESOURCE, 0, 0);
	setupKeyboardNavigation();
	
	// Set accessibility info for the panel
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("panel")
		.withName("Resources panel")
		.withDescription("Use arrow keys to navigate resources"));
}

ArtifactsPanel::ArtifactsPanel(const CTradeableItem::ClickPressedFunctor & clickPressedCallback,
	const UpdateSlotsFunctor & updateSubtitles, const std::vector<TradeItemBuy> & arts)
{
	assert(slotsForTrade == slotsPos.size());
	assert(slotsForTrade == arts.size());
	OBJECT_CONSTRUCTION;

	for(auto slotIdx = 0; slotIdx < slotsForTrade; slotIdx++)
	{
		auto artType = arts[slotIdx].getNum();
		if(artType != ArtifactID::NONE)
		{
			auto slot = slots.emplace_back(std::make_shared<CTradeableItem>(Rect(slotsPos[slotIdx], slotDimension),
				EType::ARTIFACT_TYPE, artType, slotIdx));
			slot->clickPressedCallback = clickPressedCallback;
			slot->setSelectionWidth(selectionWidth);
		}
	}
	updateSlotsCallback = updateSubtitles;
	showcaseSlot = std::make_shared<CTradeableItem>(Rect(selectedPos, slotDimension), EType::ARTIFACT_TYPE, 0, 0);
	showcaseSlot->subtitle->moveBy(Point(0, 1));
	setupKeyboardNavigation();
}

PlayersPanel::PlayersPanel(const CTradeableItem::ClickPressedFunctor & clickPressedCallback)
{
	assert(PlayerColor::PLAYER_LIMIT_I <= slotsPos.size() + 1);
	OBJECT_CONSTRUCTION;

	std::vector<PlayerColor> players;
	for(auto player = PlayerColor(0); player < PlayerColor::PLAYER_LIMIT_I; player++)
	{
		if(player != GAME->interface()->playerID && GAME->interface()->cb->getPlayerStatus(player) == EPlayerStatus::INGAME)
			players.emplace_back(player);
	}

	slots.resize(players.size());
	int slotNum = 0;
	for(auto & slot : slots)
	{
		slot = std::make_shared<CTradeableItem>(Rect(slotsPos[slotNum], slotDimension), EType::PLAYER, players[slotNum].num, slotNum);
		slot->clickPressedCallback = clickPressedCallback;
		slot->setSelectionWidth(selectionWidth);
		slot->subtitle->setText(LIBRARY->generaltexth->capColors[players[slotNum].num]);
		slotNum++;
	}
	showcaseSlot = std::make_shared<CTradeableItem>(Rect(selectedPos, slotDimension), EType::PLAYER, 0, 0);
	setupKeyboardNavigation();
}

CreaturesPanel::CreaturesPanel(const CTradeableItem::ClickPressedFunctor & clickPressedCallback, const slotsData & initialSlots)
{
	assert(initialSlots.size() <= GameConstants::ARMY_SIZE);
	assert(slotsPos.size() <= GameConstants::ARMY_SIZE);
	OBJECT_CONSTRUCTION;

	for(const auto & [creatureId, slotId, creaturesNum] : initialSlots)
	{
		auto slot = slots.emplace_back(std::make_shared<CTradeableItem>(Rect(slotsPos[slotId.num], slotDimension),
			EType::CREATURE, creaturesNum == 0 ? -1 : creatureId.num, slotId));
		slot->clickPressedCallback = clickPressedCallback;
		if(creaturesNum != 0)
			slot->subtitle->setText(std::to_string(creaturesNum));
		slot->setSelectionWidth(selectionWidth);
	}
	showcaseSlot = std::make_shared<CTradeableItem>(Rect(selectedPos, slotDimension), EType::CREATURE, 0, 0);
	setupKeyboardNavigation();
}

CreaturesPanel::CreaturesPanel(const CTradeableItem::ClickPressedFunctor & clickPressedCallback,
	const std::vector<std::shared_ptr<CTradeableItem>> & srcSlots, bool emptySlots)
{
	assert(slots.size() <= GameConstants::ARMY_SIZE);
	OBJECT_CONSTRUCTION;

	for(const auto & srcSlot : srcSlots)
	{
		auto slot = slots.emplace_back(std::make_shared<CTradeableItem>(Rect(slotsPos[srcSlot->serial], srcSlot->pos.dimensions()),
			EType::CREATURE, emptySlots ? -1 : srcSlot->id, srcSlot->serial));
		slot->clickPressedCallback = clickPressedCallback;
		slot->subtitle->setText(emptySlots ? "" : srcSlot->subtitle->getText());
		slot->setSelectionWidth(selectionWidth);
	}
	showcaseSlot = std::make_shared<CTradeableItem>(Rect(selectedPos, slotDimension), EType::CREATURE, 0, 0);
	setupKeyboardNavigation();
}

ArtifactsAltarPanel::ArtifactsAltarPanel(const CTradeableItem::ClickPressedFunctor & clickPressedCallback)
{
	OBJECT_CONSTRUCTION;

	int slotNum = 0;
	for(auto & altarSlotPos : slotsPos)
	{
		auto slot = slots.emplace_back(std::make_shared<CTradeableItem>(Rect(altarSlotPos, Point(44, 44)), EType::ARTIFACT, -1, slotNum));
		slot->clickPressedCallback = clickPressedCallback;
		slot->subtitle->clear();
		slot->subtitle->moveBy(Point(0, -1));
		slotNum++;
	}
	showcaseSlot = std::make_shared<CTradeableItem>(Rect(selectedPos, slotDimension), EType::ARTIFACT_TYPE, 0, 0);
	showcaseSlot->subtitle->moveBy(Point(0, 3));
}

void TradePanelBase::onFocusGained()
{
	CIntObject::onFocusGained();
	// Panel gained focus - ensure a slot is focused if possible
	if(focusedSlotIndex < 0 || focusedSlotIndex >= slots.size())
	{
		// Find first valid slot
		for(int i = 0; i < slots.size(); i++)
		{
			if(slots[i]->id >= 0)
			{
				moveFocusToSlot(i);
				break;
			}
		}
	}
}

void TradePanelBase::onFocusLost()
{
	CIntObject::onFocusLost();
	// Panel lost focus - clear slot focus
	if(focusedSlotIndex >= 0 && focusedSlotIndex < slots.size())
	{
		slots[focusedSlotIndex]->setFocus(false);
	}
}
