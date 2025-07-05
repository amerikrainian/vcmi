/*
 * CArtifactsOfHeroBackpack.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "CArtifactsOfHeroBackpack.h"

#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/Shortcut.h"
#include "../gui/AccessibilityManager.h"

#include "Images.h"
#include "IGameSettings.h"
#include "ObjectLists.h"

#include "../CPlayerInterface.h"
#include "../../lib/entities/artifact/ArtifactUtils.h"
#include "../../lib/entities/artifact/CArtifact.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/mapObjects/CGHeroInstance.h"
#include "../../lib/networkPacks/ArtifactLocation.h"

CArtifactsOfHeroBackpack::CArtifactsOfHeroBackpack(size_t slotsColumnsMax, size_t slotsRowsMax)
	: slotsColumnsMax(slotsColumnsMax)
	, slotsRowsMax(slotsRowsMax)
	, backpackPos(0)
{
	setRedrawParent(true);
	addUsedEvents(KEYBOARD);
}

CArtifactsOfHeroBackpack::CArtifactsOfHeroBackpack()
	: CArtifactsOfHeroBackpack(8, 8)
{
	const auto backpackCap = GAME->interface()->cb->getSettings().getInteger(EGameSettings::HEROES_BACKPACK_CAP);
	auto visibleCapacityMax = slotsRowsMax * slotsColumnsMax;
	if(backpackCap >= 0)
		visibleCapacityMax = visibleCapacityMax > backpackCap ? backpackCap : visibleCapacityMax;

	// Clear any backpack slots created by base class init()
	backpack.clear();
	
	initAOHbackpack(visibleCapacityMax, backpackCap < 0 || visibleCapacityMax < backpackCap);
	setClickPressedArtPlacesCallback(std::bind(&CArtifactsOfHeroBase::clickPressedArtPlace, this, _1, _2));
	setShowPopupArtPlacesCallback(std::bind(&CArtifactsOfHeroBase::showPopupArtPlace, this, _1, _2));
}

void CArtifactsOfHeroBackpack::onSliderMoved(int newVal)
{
	backpackPos += newVal;
	updateBackpackSlots();
}

void CArtifactsOfHeroBackpack::updateBackpackSlots()
{
	if(backpackListBox)
		backpackListBox->resize(getActiveSlotRowsNum());
	auto slot = ArtifactPosition::BACKPACK_START + backpackPos;
	for(const auto & artPlace : backpack)
	{
		setSlotData(artPlace, slot);
		slot = slot + 1;
	}
	redraw();
}

size_t CArtifactsOfHeroBackpack::getActiveSlotRowsNum()
{
	return (curHero->artifactsInBackpack.size() + slotsColumnsMax - 1) / slotsColumnsMax;
}

size_t CArtifactsOfHeroBackpack::getSlotsNum()
{
	return backpack.size();
}

void CArtifactsOfHeroBackpack::initAOHbackpack(size_t slots, bool slider)
{
	OBJECT_CONSTRUCTION;

	// Clear any backpack slots created by the base class
	backpack.clear();
	backpack.resize(slots);
	size_t artPlaceIdx = 0;
	for(auto & artPlace : backpack)
	{
		const auto pos = Point(slotSizeWithMargin * (artPlaceIdx % slotsColumnsMax),
			slotSizeWithMargin * (artPlaceIdx / slotsColumnsMax));
		backpackSlotsBackgrounds.emplace_back(std::make_shared<CPicture>(ImagePath::builtin("heroWindow/artifactSlotEmpty"), pos));
		artPlace = std::make_shared<CArtPlace>(pos, ArtifactID::NONE, SpellID::NONE, false); // false = not focusable
		
		// Clear tab order to prevent individual slots from being tabbable
		artPlace->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("artifact_slot")
			.withName("Backpack slot")
			.withDescription("Part of backpack grid")
			.withState("empty"));
		
		artPlaceIdx++;
	}

	if(slider)
	{
		auto onCreate = [](size_t index) -> std::shared_ptr<CIntObject>
		{
			return std::make_shared<CIntObject>();
		};
		CListBoxWithCallback::MovedPosCallback posMoved = [this](size_t pos) -> void
		{
			onSliderMoved(static_cast<int>(pos) * slotsColumnsMax - backpackPos);
		};
		backpackListBox = std::make_shared<CListBoxWithCallback>(
			posMoved, onCreate, Point(0, 0), Point(0, 0), slotsRowsMax, 0, 0, 1,
			Rect(slotsColumnsMax * slotSizeWithMargin + sliderPosOffsetX, 0, slotsRowsMax * slotSizeWithMargin - 2, 0));
	}

	pos.w = slots > slotsColumnsMax ? slotsColumnsMax : slots;
	pos.w *= slotSizeWithMargin;
	if(slider)
		pos.w += sliderPosOffsetX + 16; // 16 is slider width. TODO: get it from CListBox directly;
	pos.h = calcRows(slots) * slotSizeWithMargin;
	
	// Make the backpack widget itself focusable as a single unit
	addUsedEvents(KEYBOARD);
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("grid")
		.withName("Artifact backpack grid")
		.withDescription("Use arrow keys to navigate between artifacts. Press Enter to pick up an artifact, Backspace for details")
		.withTabOrder(10));
}

size_t CArtifactsOfHeroBackpack::calcRows(size_t slots)
{
	size_t rows = 0;
	if(slotsColumnsMax != 0)
	{
		rows = slots / slotsColumnsMax;
		if(slots % slotsColumnsMax != 0)
			rows += 1;
	}
	return rows;
}

bool CArtifactsOfHeroBackpack::captureThisKey(EShortcut key)
{
	// Don't capture Tab navigation keys - let the focus system handle them
	if(key == EShortcut::GLOBAL_MOVE_FOCUS || key == EShortcut::GLOBAL_MOVE_FOCUS_PREV)
		return false;
		
	// Only capture keys when we have focus and grid navigation is enabled
	if (!hasFocus() || !gridNavigationEnabled)
		return false;
		
	// Capture arrow keys and accept key
	return key == EShortcut::MOVE_UP || 
	       key == EShortcut::MOVE_DOWN ||
	       key == EShortcut::MOVE_LEFT ||
	       key == EShortcut::MOVE_RIGHT ||
	       key == EShortcut::GLOBAL_ACCEPT ||
	       key == EShortcut::GLOBAL_BACKSPACE ||
	       key == EShortcut::ARTIFACT_MOVE_TO_BACKPACK ||
	       key == EShortcut::ARTIFACT_TRANSFER_TO_HERO;
}

void CArtifactsOfHeroBackpack::keyPressed(EShortcut key)
{
	// Debug: Log key press
	logGlobal->info("CArtifactsOfHeroBackpack: keyPressed called with key %d, gridNavigationEnabled=%d, hasFocus=%d", 
		static_cast<int>(key), gridNavigationEnabled, hasFocus());
	
	if (!gridNavigationEnabled)
	{
		// Let parent handle normal tab navigation
		CArtifactsOfHeroBase::keyPressed(key);
		return;
	}
	
	bool handled = true;
	Point newFocus = focusedCell;
	
	switch(key)
	{
	case EShortcut::MOVE_UP:
		if (focusedCell.y > 0)
			newFocus.y--;
		else
			return; // Stop silently at edge
		break;
	case EShortcut::MOVE_DOWN:
		if (focusedCell.y < static_cast<int>(slotsRowsMax) - 1)
			newFocus.y++;
		else
			return; // Stop silently at edge
		break;
	case EShortcut::MOVE_LEFT:
		if (focusedCell.x > 0)
			newFocus.x--;
		else
			return; // Stop silently at edge
		break;
	case EShortcut::MOVE_RIGHT:
		if (focusedCell.x < static_cast<int>(slotsColumnsMax) - 1)
			newFocus.x++;
		else
			return; // Stop silently at edge
		break;
	case EShortcut::GLOBAL_ACCEPT:
	case EShortcut::SELECT_INDEX_1:
		{
			// Activate the focused artifact slot
			int slotIndex = focusedCell.y * slotsColumnsMax + focusedCell.x;
			if (slotIndex < static_cast<int>(backpack.size()) && backpack[slotIndex])
			{
				backpack[slotIndex]->clickPressed(backpack[slotIndex]->pos.center());
			}
		}
		return;
	case EShortcut::MOUSE_RIGHT:
	case EShortcut::GLOBAL_BACKSPACE:
		{
			// Show context menu for focused artifact
			int slotIndex = focusedCell.y * slotsColumnsMax + focusedCell.x;
			if (slotIndex < static_cast<int>(backpack.size()) && backpack[slotIndex])
			{
				backpack[slotIndex]->showPopupWindow(backpack[slotIndex]->pos.center());
			}
		}
		return;
	case EShortcut::ARTIFACT_MOVE_TO_BACKPACK:
	case EShortcut::ARTIFACT_TRANSFER_TO_HERO:
		{
			// Handle artifact shortcuts in grid mode
			int slotIndex = focusedCell.y * slotsColumnsMax + focusedCell.x;
			if (slotIndex < static_cast<int>(backpack.size()) && backpack[slotIndex])
			{
				// Temporarily enable keyboard events for the shortcut handling
				backpack[slotIndex]->addUsedEvents(KEYBOARD);
				backpack[slotIndex]->keyPressed(key);
				backpack[slotIndex]->removeUsedEvents(KEYBOARD);
			}
		}
		return;
	default:
		handled = false;
		break;
	}
	
	if (handled)
	{
		// Validate new position is within the visible grid
		int newSlotIndex = newFocus.y * slotsColumnsMax + newFocus.x;
		if (newSlotIndex < static_cast<int>(backpack.size()))
		{
			focusedCell = newFocus;
			updateFocusedArtifact();
		}
	}
	else
	{
		// Pass unhandled keys to parent
		CArtifactsOfHeroBase::keyPressed(key);
	}
}

CArtifactsOfHeroQuickBackpack::CArtifactsOfHeroQuickBackpack(const ArtifactPosition filterBySlot)
	: CArtifactsOfHeroBackpack(0, 0)
{
	if(!ArtifactUtils::isSlotEquipment(filterBySlot))
		return;

	this->filterBySlot = filterBySlot;
	setShowPopupArtPlacesCallback(std::bind(&CArtifactsOfHeroBase::showPopupArtPlace, this, _1, _2));
}

void CArtifactsOfHeroQuickBackpack::setHero(const CGHeroInstance * hero)
{
	if(curHero == hero)
		return;
	
	curHero = hero;
	if(curHero)
	{
		ArtifactID artInSlotId = ArtifactID::NONE;
		SpellID scrollInSlotSpellId = SpellID::NONE;
		if(auto artInSlot = curHero->getArt(filterBySlot))
		{
			artInSlotId = artInSlot->getTypeId();
			scrollInSlotSpellId = artInSlot->getScrollSpellID();
		}

		std::map<const ArtifactID, const CArtifactInstance*> filteredArts;
		for(auto & slotInfo : curHero->artifactsInBackpack)
			if(slotInfo.getArt()->getTypeId() != artInSlotId &&	!slotInfo.getArt()->isScroll() &&
				slotInfo.getArt()->getType()->canBePutAt(curHero, filterBySlot, true))
			{
				filteredArts.insert(std::pair(slotInfo.getArt()->getTypeId(), slotInfo.getArt()));
			}

		std::map<const SpellID, const CArtifactInstance*> filteredScrolls;
		if(filterBySlot == ArtifactPosition::MISC1 || filterBySlot == ArtifactPosition::MISC2 || filterBySlot == ArtifactPosition::MISC3 ||
			filterBySlot == ArtifactPosition::MISC4 || filterBySlot == ArtifactPosition::MISC5)
		{
			for(auto & slotInfo : curHero->artifactsInBackpack)
			{
				if(slotInfo.getArt()->isScroll() && slotInfo.getArt()->getScrollSpellID() != scrollInSlotSpellId)
					filteredScrolls.insert(std::pair(slotInfo.getArt()->getScrollSpellID(), slotInfo.getArt()));
			}
		}

		backpack.clear();
		auto requiredSlots = filteredArts.size() + filteredScrolls.size();
		slotsColumnsMax = ceilf(sqrtf(requiredSlots));
		slotsRowsMax = calcRows(requiredSlots);
		initAOHbackpack(requiredSlots, false);
		setClickPressedArtPlacesCallback(std::bind(&CArtifactsOfHeroBase::clickPressedArtPlace, this, _1, _2));
		auto artPlace = backpack.begin();
		for(auto & art : filteredArts)
			setSlotData(*artPlace++, curHero->getArtPos(art.second));
		for(auto & art : filteredScrolls)
			setSlotData(*artPlace++, curHero->getArtPos(art.second));
	}
}

ArtifactPosition CArtifactsOfHeroQuickBackpack::getFilterSlot()
{
	return filterBySlot;
}

void CArtifactsOfHeroQuickBackpack::selectSlotAt(const Point & position)
{
	for(auto & artPlace : backpack)
		artPlace->selectSlot(artPlace->pos.isInside(position));
}

void CArtifactsOfHeroQuickBackpack::swapSelected()
{
	ArtifactLocation backpackLoc(curHero->id, ArtifactPosition::PRE_FIRST);
	for(auto & artPlace : backpack)
		if(artPlace->isSelected())
		{
			backpackLoc.slot = artPlace->slot;
			break;
		}
	if(backpackLoc.slot != ArtifactPosition::PRE_FIRST && filterBySlot != ArtifactPosition::PRE_FIRST && curHero)
		GAME->interface()->cb->swapArtifacts(backpackLoc, ArtifactLocation(curHero->id, filterBySlot));
}

void CArtifactsOfHeroBackpack::onFocusGained()
{
	gridNavigationEnabled = true;
	focusedCell = {0, 0}; // Start at top-left
	updateFocusedArtifact();
	
	// Debug: Log focus gained
	logGlobal->info("CArtifactsOfHeroBackpack: Focus gained, grid navigation enabled");
}

void CArtifactsOfHeroBackpack::onFocusLost()
{
	gridNavigationEnabled = false;
	// Clear visual focus from all artifacts
	for (auto & artPlace : backpack)
	{
		if (artPlace)
			artPlace->hover(false);
	}
}

void CArtifactsOfHeroBackpack::updateFocusedArtifact()
{
	// Clear previous focus
	for (auto & artPlace : backpack)
	{
		if (artPlace)
			artPlace->hover(false);
	}
	
	// Set new focus
	int slotIndex = focusedCell.y * slotsColumnsMax + focusedCell.x;
	if (slotIndex < static_cast<int>(backpack.size()) && backpack[slotIndex])
	{
		backpack[slotIndex]->hover(true);
		announceFocusedArtifact();
	}
}

void CArtifactsOfHeroBackpack::announceFocusedArtifact()
{
	int slotIndex = focusedCell.y * slotsColumnsMax + focusedCell.x;
	logGlobal->info("announceFocusedArtifact: slotIndex=%d, backpack.size=%d, focusedCell=(%d,%d)", 
		slotIndex, backpack.size(), focusedCell.x, focusedCell.y);
	
	if (slotIndex >= static_cast<int>(backpack.size()) || !backpack[slotIndex])
	{
		logGlobal->info("announceFocusedArtifact: Invalid slot index or null slot");
		return;
	}
	
	auto artPlace = backpack[slotIndex];
	if (!artPlace || artPlace->getArtifactId() == ArtifactID::NONE)
	{
		logGlobal->info("announceFocusedArtifact: Empty slot");
		AccessibilityManager::getInstance().announce("Empty slot", true);
		return;
	}
	
	// Get the actual artifact from the hero's backpack
	ArtifactPosition actualPos = ArtifactPosition::BACKPACK_START + backpackPos + slotIndex;
	const auto art = curHero->getArt(actualPos);
	if (art)
	{
		std::string announcement = art->getType()->getNameTranslated();
		announcement += ". Row " + std::to_string(focusedCell.y + 1);
		announcement += ", Column " + std::to_string(focusedCell.x + 1);
		logGlobal->info("announceFocusedArtifact: Announcing artifact: %s", announcement.c_str());
		AccessibilityManager::getInstance().announce(announcement, true);
	}
	else
	{
		logGlobal->info("announceFocusedArtifact: No artifact at position %d", actualPos.num);
	}
}
