/*
 * CArtifactsOfHeroBackpack.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "CArtifactsOfHeroBase.h"

VCMI_LIB_NAMESPACE_BEGIN

struct ArtifactLocation;

VCMI_LIB_NAMESPACE_END

class CListBoxWithCallback;

class CArtifactsOfHeroBackpack : public CArtifactsOfHeroBase
{
public:
	CArtifactsOfHeroBackpack(size_t slotsColumnsMax, size_t slotsRowsMax);
	CArtifactsOfHeroBackpack();
	void onSliderMoved(int newVal);
	void updateBackpackSlots() override;
	size_t getActiveSlotRowsNum();
	size_t getSlotsNum();
	void keyPressed(EShortcut key) override;
	bool captureThisKey(EShortcut key) override;
	void onFocusGained() override;
	void onFocusLost() override;
	bool isFocusable() const override { return true; }

protected:
	std::shared_ptr<CListBoxWithCallback> backpackListBox;
	std::vector<std::shared_ptr<CPicture>> backpackSlotsBackgrounds;
	size_t slotsColumnsMax;
	size_t slotsRowsMax;
	const int slotSizeWithMargin = 46;
	const int sliderPosOffsetX = 5;
	int backpackPos; // Position to display artifacts in heroes backpack
	
	// Grid navigation
	Point focusedCell = {0, 0}; // Current focused cell in grid (x=column, y=row)
	bool gridNavigationEnabled = false;

	void initAOHbackpack(size_t slots, bool slider);
	size_t calcRows(size_t slots);
	void updateFocusedArtifact();
	void announceFocusedArtifact();
};

class CArtifactsOfHeroQuickBackpack : public CArtifactsOfHeroBackpack
{
public:
	CArtifactsOfHeroQuickBackpack(const ArtifactPosition filterBySlot);
	void setHero(const CGHeroInstance * hero);
	ArtifactPosition getFilterSlot();
	void selectSlotAt(const Point & position);
	void swapSelected();

private:
	ArtifactPosition filterBySlot;
};
