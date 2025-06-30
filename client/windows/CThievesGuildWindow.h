/*
 * CThievesGuildWindow.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../gui/CIntObject.h"
#include "CWindowObject.h"
#include "../../lib/GameConstants.h"
#include "../../lib/gameState/SThievesGuildInfo.h"

VCMI_LIB_NAMESPACE_BEGIN
class CGObjectInstance;
VCMI_LIB_NAMESPACE_END

class CButton;
class CGStatusBar;
class CMinorResDataBar;
class CLabel;
class CAnimImage;
class CPicture;
class LRClickableArea;
class CThievesGuildGrid;

/// Grid cell for navigating the ranking table
class CThievesGuildCell : public CIntObject
{
public:
	int row;
	int column;
	std::string content;
	std::vector<PlayerColor> players;
	
	CThievesGuildCell(const Rect& rect, int row, int col, const std::string& content, const std::vector<PlayerColor>& players);
	
	void clickPressed(const Point & cursorPosition) override;
	void showPopupWindow(const Point & cursorPosition) override;
	std::string getHoverText() const;
	void onFocusGained() override;
	void onFocusLost() override;
	void announceContent();
};

/// Thieves Guild window implementation with full accessibility
class CThievesGuildWindow : public CWindowObject
{
	friend class CThievesGuildGrid;
	const CGObjectInstance * owner;

	std::shared_ptr<CGStatusBar> statusbar;
	std::shared_ptr<CButton> exitb;
	std::shared_ptr<CMinorResDataBar> resdatabar;

	// Visual elements
	std::vector<std::shared_ptr<CLabel>> rowHeaders;
	std::vector<std::shared_ptr<CAnimImage>> columnBackgrounds;
	std::vector<std::shared_ptr<CLabel>> columnHeaders;
	std::vector<std::shared_ptr<CAnimImage>> columnHeaderIcons;
	std::vector<std::shared_ptr<CAnimImage>> cells;

	std::vector<std::shared_ptr<CPicture>> banners;
	std::vector<std::shared_ptr<CAnimImage>> bestHeroes;
	std::vector<std::shared_ptr<CLabel>> primSkillHeaders;
	std::vector<std::shared_ptr<LRClickableArea>> primSkillHeadersArea;
	std::vector<std::shared_ptr<CLabel>> primSkillValues;
	std::vector<std::shared_ptr<CAnimImage>> bestCreatures;
	std::vector<std::shared_ptr<CLabel>> personalities;
	
	// Accessibility elements
	std::vector<std::vector<std::shared_ptr<CThievesGuildCell>>> gridCells;
	std::vector<std::shared_ptr<CIntObject>> bestHeroCards;
	std::vector<std::shared_ptr<CIntObject>> personalityCards;
	std::shared_ptr<CIntObject> gridContainer;
	
	// Navigation state
	int currentRow = 0;
	int currentCol = 0;
	enum NavigationMode { GRID, BEST_HEROES, PERSONALITIES } navMode = GRID;
	int currentHeroIndex = 0;
	int currentPersonalityIndex = 0;
	
	// Data
	SThievesGuildInfo tgi;
	
	void navigateGrid(int deltaRow, int deltaCol);
	void navigateToBestHeroes();
	void navigateToPersonalities();
	void updateFocus();
	
public:
	CThievesGuildWindow(const CGObjectInstance * _owner);
	
	void keyPressed(EShortcut key) override;
	void show(Canvas & to) override;
};