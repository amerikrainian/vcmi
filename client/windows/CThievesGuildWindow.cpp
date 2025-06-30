/*
 * CThievesGuildWindow.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "GUIClasses.h"
#include "CThievesGuildWindow.h"

#include "../GameEngine.h"
#include "../CPlayerInterface.h"
#include "../gui/WindowHandler.h"
#include "../gui/Shortcut.h"
#include "../gui/CursorHandler.h"
#include "../gui/AccessibilityManager.h"
#include "../widgets/Buttons.h"
#include "../widgets/MiscWidgets.h"
#include "../widgets/TextControls.h"
#include "../widgets/Images.h"
#include "../render/Canvas.h"
#include "../render/ImageLocator.h"
#include "InfoWindows.h"

#include "../GameInstance.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/mapObjects/CGObjectInstance.h"
#include "../../lib/gameState/SThievesGuildInfo.h"
#include "../../lib/gameState/InfoAboutArmy.h"
#include "../../lib/IGameSettings.h"
#include "../../lib/GameSettings.h"
#include "../../lib/filesystem/ResourcePath.h"
#include "../../lib/CConfigHandler.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>


// Grid cell implementation
CThievesGuildCell::CThievesGuildCell(const Rect& rect, int row, int col, const std::string& content, const std::vector<PlayerColor>& players)
	: CIntObject(), row(row), column(col), content(content), players(players)
{
	pos = rect;
	setRedrawParent(true);
	
	// Grid cells don't need keyboard events or tab order - the grid container handles navigation
	// Just set accessibility info for screen reader announcements
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("gridcell")
		.withName(content)
		.withDescription("Row " + std::to_string(row + 1) + ", Column " + std::to_string(col + 1)));
}

void CThievesGuildCell::clickPressed(const Point & cursorPosition)
{
	// Announce content when clicked
	announceContent();
}

void CThievesGuildCell::showPopupWindow(const Point & cursorPosition)
{
	// Show detailed information in a popup
	if (!players.empty())
	{
		std::string text = content + "\n\nPlayers:\n";
		for (auto & player : players)
		{
			text += LIBRARY->generaltexth->capColors[player.getNum()] + "\n";
		}
		CRClickPopup::createAndPush(text);
	}
}

std::string CThievesGuildCell::getHoverText() const
{
	return content;
}

void CThievesGuildCell::onFocusGained()
{
	CIntObject::onFocusGained();
	announceContent();
}

void CThievesGuildCell::onFocusLost()
{
	CIntObject::onFocusLost();
}

void CThievesGuildCell::announceContent()
{
	std::string announcement = content + ". ";
	if (!players.empty())
	{
		announcement += std::to_string(players.size()) + " player";
		if (players.size() > 1) announcement += "s";
		announcement += ": ";
		
		for (size_t i = 0; i < players.size(); ++i)
		{
			announcement += LIBRARY->generaltexth->capColors[players[i].getNum()];
			if (i < players.size() - 1) announcement += ", ";
		}
	}
	else
	{
		announcement += "No data";
	}
	
	AccessibilityManager::getInstance().announce(announcement);
}

// Best hero card implementation
class CBestHeroCard : public CIntObject
{
	PlayerColor player;
	const InfoAboutHero* heroInfo;
	int index;
	
public:
	CBestHeroCard(const Rect& rect, PlayerColor player, const InfoAboutHero* info, int index)
		: player(player), heroInfo(info), index(index)
	{
		pos = rect;
		setRedrawParent(true);
		
		// Make focusable - must add KEYBOARD events
		addUsedEvents(KEYBOARD);
		
		std::string heroName = info && info->name.length() ? info->name : "Unknown hero";
		
		setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("card")
			.withName(LIBRARY->generaltexth->capColors[player.getNum()] + " best hero: " + heroName)
			.withDescription("Best hero information")
			.withTabOrder(100 + index));
	}
	
	void onFocusGained() override
	{
		CIntObject::onFocusGained();
			std::string announcement = LIBRARY->generaltexth->capColors[player.getNum()] + " best hero: ";
			if (heroInfo && heroInfo->name.length())
			{
				announcement += heroInfo->name;
				if (heroInfo->details)
				{
					announcement += ". Primary skills: ";
					announcement += "Attack " + std::to_string(heroInfo->details->primskills[0]) + ", ";
					announcement += "Defense " + std::to_string(heroInfo->details->primskills[1]) + ", ";
					announcement += "Spell Power " + std::to_string(heroInfo->details->primskills[2]) + ", ";
					announcement += "Knowledge " + std::to_string(heroInfo->details->primskills[3]);
				}
			}
			else
			{
				announcement += "Unknown";
			}
			
			AccessibilityManager::getInstance().announce(announcement);
	}
	
	void clickPressed(const Point & cursorPosition) override
	{
		setFocus(true);
	}
};

// Personality card implementation
class CPersonalityCard : public CIntObject
{
	PlayerColor player;
	EAiTactic personality;
	int index;
	
public:
	CPersonalityCard(const Rect& rect, PlayerColor player, EAiTactic personality, int index)
		: player(player), personality(personality), index(index)
	{
		pos = rect;
		setRedrawParent(true);
		
		// Make focusable - must add KEYBOARD events
		addUsedEvents(KEYBOARD);
		
		std::string personalityText;
		if (personality == EAiTactic::NONE)
			personalityText = LIBRARY->generaltexth->arraytxt[172];
		else if (personality != EAiTactic::RANDOM)
			personalityText = LIBRARY->generaltexth->arraytxt[168 + static_cast<int>(personality)];
		else
			personalityText = "Random";
			
		setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("card")
			.withName(LIBRARY->generaltexth->capColors[player.getNum()] + " personality: " + personalityText)
			.withTabOrder(200 + index));
	}
	
	void onFocusGained() override
	{
		CIntObject::onFocusGained();
			std::string announcement = LIBRARY->generaltexth->capColors[player.getNum()] + " AI personality: ";
			if (personality == EAiTactic::NONE)
				announcement += LIBRARY->generaltexth->arraytxt[172];
			else if (personality != EAiTactic::RANDOM)
				announcement += LIBRARY->generaltexth->arraytxt[168 + static_cast<int>(personality)];
			else
				announcement += "Random";
				
			AccessibilityManager::getInstance().announce(announcement);
	}
	
	void clickPressed(const Point & cursorPosition) override
	{
		setFocus(true);
	}
};

// Grid container that handles arrow key navigation
class CThievesGuildGrid : public CIntObject
{
	CThievesGuildWindow* parent;
	
public:
	CThievesGuildGrid(CThievesGuildWindow* parent, const Rect& rect)
		: parent(parent)
	{
		pos = rect;
		
		// Make focusable with a single tab order
		addUsedEvents(KEYBOARD);
		setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("grid")
			.withName("Player rankings grid")
			.withDescription("Use arrow keys to navigate rankings")
			.withTabOrder(10));
	}
	
	void keyPressed(EShortcut key) override
	{
		// Only handle arrow keys, let Tab pass through
		switch(key)
		{
			case EShortcut::MOVE_UP:
			case EShortcut::MOVE_DOWN:
			case EShortcut::MOVE_LEFT:
			case EShortcut::MOVE_RIGHT:
				parent->keyPressed(key);
				break;
			default:
				// Let other keys (including Tab) pass through
				CIntObject::keyPressed(key);
				break;
		}
	}
	
	void onFocusGained() override
	{
		CIntObject::onFocusGained();
		parent->navMode = CThievesGuildWindow::GRID;
		parent->updateFocus();
		AccessibilityManager::getInstance().announce("Rankings grid. Use arrow keys to navigate.", true);
	}
};

// Main window implementation
CThievesGuildWindow::CThievesGuildWindow(const CGObjectInstance * _owner):
	CWindowObject(PLAYER_COLORED | BORDERED, ImagePath::builtin("TpRank")),
	owner(_owner)
{
	OBJECT_CONSTRUCTION;
	
	// Enable keyboard events for the window
	addUsedEvents(KEYBOARD);
	
	// Set accessibility info for the window
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("dialog")
		.withName("Thieves Guild")
		.withDescription("View player rankings and statistics"));

	// Get thieves guild info
	GAME->interface()->cb->getThievesGuildInfo(tgi, owner);

	// Create exit button
	exitb = std::make_shared<CButton>(Point(748, 556), AnimationPath::builtin("TPMAGE1"), CButton::tooltip(LIBRARY->generaltexth->allTexts[600]), [&](){ close();}, EShortcut::GLOBAL_RETURN);
	exitb->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Exit")
		.withDescription("Close thieves guild window")
		.withTabOrder(300));
		
	statusbar = CGStatusBar::create(3, 555, ImagePath::builtin("TStatBar.bmp"), 742);
	resdatabar = std::make_shared<CMinorResDataBar>();
	resdatabar->moveBy(pos.topLeft(), true);

	// Row headers
	const std::vector<std::string> rowNames = {
		"Number of Towns", "Number of Heroes", "Gold",
		"Wood and Ore", "Mercury, Sulfur, Crystal and Gems", "Obelisks Found",
		"Artifacts", "Army Strength", "Income"
	};
	
	// Column headers (player names)
	std::vector<std::string> columnNames;
	for (int i = 0; i < tgi.playerColors.size(); ++i)
	{
		columnNames.push_back(LIBRARY->generaltexth->jktexts[16 + i]);
	}
	
	// Create visual elements (keeping existing code)
	for(int g=0; g<12; ++g)
	{
		int posY[] = {400, 460, 510};
		int y;
		if(g < 9)
			y = 52 + 32*g;
		else
			y = posY[g-9];

		std::string text = LIBRARY->generaltexth->jktexts[24+g];
		boost::algorithm::trim_if(text,boost::algorithm::is_any_of("\""));
		
		if(settings["general"]["enableUiEnhancements"].Bool() && g >= 2 && g <= 4)
		{
			auto addicon = [this, y](GameResID res, int x){ 
				columnHeaderIcons.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("SMALRES"), res.getNum(), 0, x, y - 10)); 
			};
			
			if(g == 2) // gold
				addicon(GameResID::GOLD, 125);
			else if(g == 3) // wood, ore
			{
				addicon(GameResID::WOOD, 110);
				addicon(GameResID::ORE, 140);
			}
			else if(g == 4) // mercury, sulfur, crystal, gems
			{
				addicon(GameResID::MERCURY, 80);
				addicon(GameResID::SULFUR, 110);
				addicon(GameResID::CRYSTAL, 140);
				addicon(GameResID::GEMS, 170);
			}
		}
		else
			rowHeaders.push_back(std::make_shared<CLabel>(135, y, FONT_MEDIUM, ETextAlignment::CENTER, Colors::YELLOW, text, 220));
	}

	// Column backgrounds and headers
	for(int g=1; g<tgi.playerColors.size(); ++g)
		columnBackgrounds.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("PRSTRIPS"), g-1, 0, 250 + 66*g, 7));

	for(int g=0; g<tgi.playerColors.size(); ++g)
		columnHeaders.push_back(std::make_shared<CLabel>(283 + 66*g, 21, FONT_BIG, ETextAlignment::CENTER, Colors::YELLOW, columnNames[g]));

	// Create the grid of focusable cells
	gridCells.resize(9); // 9 ranking categories
	
	// Data for information table
	constexpr std::vector< std::vector< PlayerColor > > SThievesGuildInfo::* fields[] =
		{ &SThievesGuildInfo::numOfTowns, &SThievesGuildInfo::numOfHeroes,       &SThievesGuildInfo::gold,
		  &SThievesGuildInfo::woodOre,    &SThievesGuildInfo::mercSulfCrystGems, &SThievesGuildInfo::obelisks,
		  &SThievesGuildInfo::artifacts,  &SThievesGuildInfo::army,              &SThievesGuildInfo::income };

	// Create visual flags and focusable cells
	for(int g = 0; g < std::size(fields); ++g) // by lines
	{
		gridCells[g].resize(tgi.playerColors.size());
		
		for(int b=0; b<(tgi .* fields[g]).size(); ++b) // by places
		{
			std::vector<PlayerColor> &players = (tgi .* fields[g])[b];

			// Position of box
			int xpos = 259 + 66 * b;
			int ypos = 41 +  32 * g;

			// Create visual flags
			size_t rowLength[2];
			rowLength[0] = std::min<size_t>(players.size(), 4);
			rowLength[1] = players.size() - rowLength[0];

			for(size_t j=0; j < 2; j++)
			{
				int rowStartX = xpos + (j ? 6 + ((int)rowLength[j] < 3 ? 12 : 0) : 24 - 6 * (int)rowLength[j]);
				int rowStartY = ypos + (j ? 4 : 0);

				for(size_t i=0; i < rowLength[j]; i++)
					cells.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("itgflags"), players[i + j*4].getNum(), 0, rowStartX + (int)i*12, rowStartY));
			}
			
			// Create focusable cell
			Rect cellRect(xpos - 8, ypos - 8, 66, 32);
			std::string cellContent = rowNames[g] + ": ";
			
			if (b < columnNames.size())
				cellContent += columnNames[b] + " - ";
				
			cellContent += "Rank " + std::to_string(b + 1);
			
			gridCells[g][b] = std::make_shared<CThievesGuildCell>(cellRect, g, b, cellContent, players);
		}
		
		// Fill empty cells
		for(int b = (tgi .* fields[g]).size(); b < tgi.playerColors.size(); ++b)
		{
			int xpos = 259 + 66 * b;
			int ypos = 41 +  32 * g;
			Rect cellRect(xpos - 8, ypos - 8, 66, 32);
			
			std::string cellContent = rowNames[g] + ": " + columnNames[b] + " - No data";
			gridCells[g][b] = std::make_shared<CThievesGuildCell>(cellRect, g, b, cellContent, std::vector<PlayerColor>());
		}
	}
	
	// Create grid container for arrow key navigation
	// The grid container covers the entire ranking grid area
	Rect gridRect(250, 33, 66 * tgi.playerColors.size(), 32 * 9);
	gridContainer = std::make_shared<CThievesGuildGrid>(this, gridRect);

	// Best heroes section
	static const std::string colorToBox[] = {"PRRED.BMP", "PRBLUE.BMP", "PRTAN.BMP", "PRGREEN.BMP", "PRORANGE.BMP", "PRPURPLE.BMP", "PRTEAL.BMP", "PRROSE.bmp"};
	
	int counter = 0;
	for(auto & iter : tgi.colorToBestHero)
	{
		int xPos = 253 + 66 * counter;
		banners.push_back(std::make_shared<CPicture>(ImagePath::builtin(colorToBox[iter.first.getNum()]), xPos, 334));
		
		if(iter.second.portraitSource.isValid())
		{
			bestHeroes.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("PortraitsSmall"), iter.second.getIconIndex(), 0, xPos + 7, 360));
			
			if(iter.second.details)
			{
				std::vector<std::string> lines;
				boost::split(lines, LIBRARY->generaltexth->allTexts[184], boost::is_any_of("\n"));
				for(int i=0; i<GameConstants::PRIMARY_SKILLS; ++i)
				{
					primSkillHeaders.push_back(std::make_shared<CLabel>(xPos + 7, 407 + 11 * i, FONT_TINY, ETextAlignment::BOTTOMLEFT, Colors::WHITE, lines[i]));
					primSkillValues.push_back(std::make_shared<CLabel>(xPos + 57, 407 + 11 * i, FONT_TINY, ETextAlignment::BOTTOMRIGHT, Colors::WHITE,
							   std::to_string(iter.second.details->primskills[i])));
				}
			}
		}
		
		// Create focusable hero card
		Rect heroRect(xPos, 334, 66, 120);
		bestHeroCards.push_back(std::make_shared<CBestHeroCard>(heroRect, iter.first, &iter.second, counter));
		
		counter++;
	}

	// Best creatures
	counter = 0;
	for(auto & it : tgi.bestCreature)
	{
		if(it.second != CreatureID::NONE)
			bestCreatures.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("TWCRPORT"), it.second+2, 0, 255 + 66 * counter, 479));
		counter++;
	}

	// Personalities section
	counter = 0;
	for(auto & it : tgi.personality)
	{
		std::string text;
		if(it.second == EAiTactic::NONE)
			text = LIBRARY->generaltexth->arraytxt[172];
		else if(it.second != EAiTactic::RANDOM)
			text = LIBRARY->generaltexth->arraytxt[168 + static_cast<int>(it.second)];

		personalities.push_back(std::make_shared<CLabel>(283 + 66*counter, 459, FONT_SMALL, ETextAlignment::CENTER, Colors::WHITE, text));
		
		// Create focusable personality card
		Rect persRect(259 + 66 * counter, 450, 66, 30);
		personalityCards.push_back(std::make_shared<CPersonalityCard>(persRect, it.first, it.second, counter));
		
		counter++;
	}
	
	// Announce window opening
	std::string announcement = "Thieves Guild window opened. Shows rankings for " + std::to_string(tgi.playerColors.size()) + " players. ";
	announcement += "Press Tab to navigate to different sections.";
	AccessibilityManager::getInstance().announce(announcement, true);
	
	// Initialize grid position but don't set focus
	currentRow = 0;
	currentCol = 0;
}

void CThievesGuildWindow::keyPressed(EShortcut key)
{
	switch(key)
	{
		case EShortcut::MOVE_UP:
			if (navMode == GRID && gridContainer && gridContainer->hasFocus() && currentRow > 0)
			{
				navigateGrid(-1, 0);
			}
			break;
			
		case EShortcut::MOVE_DOWN:
			if (navMode == GRID && gridContainer && gridContainer->hasFocus() && currentRow < gridCells.size() - 1)
			{
				navigateGrid(1, 0);
			}
			break;
			
		case EShortcut::MOVE_LEFT:
			if (navMode == GRID && gridContainer && gridContainer->hasFocus() && currentCol > 0)
			{
				navigateGrid(0, -1);
			}
			else if (navMode == BEST_HEROES && currentHeroIndex > 0)
			{
				currentHeroIndex--;
				updateFocus();
			}
			else if (navMode == PERSONALITIES && currentPersonalityIndex > 0)
			{
				currentPersonalityIndex--;
				updateFocus();
			}
			break;
			
		case EShortcut::MOVE_RIGHT:
			if (navMode == GRID && gridContainer && gridContainer->hasFocus() && currentCol < gridCells[currentRow].size() - 1)
			{
				navigateGrid(0, 1);
			}
			else if (navMode == BEST_HEROES && currentHeroIndex < bestHeroCards.size() - 1)
			{
				currentHeroIndex++;
				updateFocus();
			}
			else if (navMode == PERSONALITIES && currentPersonalityIndex < personalityCards.size() - 1)
			{
				currentPersonalityIndex++;
				updateFocus();
			}
			break;
			
		case EShortcut::GLOBAL_ACCEPT:
			// Only handle section switching in grid mode
			if (navMode == GRID)
			{
				// Announce current cell when Enter is pressed
				if (currentRow < gridCells.size() && currentCol < gridCells[currentRow].size())
				{
					gridCells[currentRow][currentCol]->announceContent();
				}
			}
			break;
			
			
		case EShortcut::SELECT_INDEX_1:
		case EShortcut::SELECT_INDEX_2:
		case EShortcut::SELECT_INDEX_3:
		case EShortcut::SELECT_INDEX_4:
		case EShortcut::SELECT_INDEX_5:
		case EShortcut::SELECT_INDEX_6:
		case EShortcut::SELECT_INDEX_7:
		case EShortcut::SELECT_INDEX_8:
			{
				// Quick jump to column
				int col = static_cast<int>(key) - static_cast<int>(EShortcut::SELECT_INDEX_1);
				if (navMode == GRID && col < gridCells[currentRow].size())
				{
					currentCol = col;
					updateFocus();
				}
			}
			break;
			
		case EShortcut::GLOBAL_CANCEL:
			if (navMode == GRID)
			{
				currentRow = 0;
				currentCol = 0;
				updateFocus();
			}
			break;
			
		case EShortcut::GLOBAL_RETURN:
			if (navMode == GRID)
			{
				currentRow = gridCells.size() - 1;
				currentCol = gridCells[currentRow].size() - 1;
				updateFocus();
			}
			break;
			
		default:
			CWindowObject::keyPressed(key);
			break;
	}
}

void CThievesGuildWindow::navigateGrid(int deltaRow, int deltaCol)
{
	currentRow += deltaRow;
	currentCol += deltaCol;
	
	// Ensure we stay in bounds
	currentRow = std::max(0, std::min(currentRow, (int)gridCells.size() - 1));
	currentCol = std::max(0, std::min(currentCol, (int)gridCells[currentRow].size() - 1));
	
	updateFocus();
}

void CThievesGuildWindow::navigateToBestHeroes()
{
	if (!bestHeroCards.empty())
	{
		navMode = BEST_HEROES;
		currentHeroIndex = 0;
		updateFocus();
		AccessibilityManager::getInstance().announce("Navigating best heroes section", true);
	}
	else
	{
		navigateToPersonalities();
	}
}

void CThievesGuildWindow::navigateToPersonalities()
{
	if (!personalityCards.empty())
	{
		navMode = PERSONALITIES;
		currentPersonalityIndex = 0;
		updateFocus();
		AccessibilityManager::getInstance().announce("Navigating AI personalities section", true);
	}
	else
	{
		// Go back to grid
		navMode = GRID;
		updateFocus();
	}
}

void CThievesGuildWindow::updateFocus()
{
	// Set new focus
	switch (navMode)
	{
		case GRID:
			// For grid mode, announce the current cell but don't change focus
			// The grid container keeps the focus
			if (currentRow < gridCells.size() && currentCol < gridCells[currentRow].size())
			{
				gridCells[currentRow][currentCol]->announceContent();
			}
			break;
			
		case BEST_HEROES:
			if (currentHeroIndex < bestHeroCards.size())
			{
				bestHeroCards[currentHeroIndex]->setFocus(true);
			}
			break;
			
		case PERSONALITIES:
			if (currentPersonalityIndex < personalityCards.size())
			{
				personalityCards[currentPersonalityIndex]->setFocus(true);
			}
			break;
	}
}

void CThievesGuildWindow::show(Canvas & to)
{
	CWindowObject::show(to);
	
	// Draw focus indicator for grid cells when grid container has focus
	if (navMode == GRID && gridContainer && gridContainer->hasFocus() 
	    && currentRow < gridCells.size() && currentCol < gridCells[currentRow].size())
	{
		auto cell = gridCells[currentRow][currentCol];
		if (cell)
		{
			// Draw focus rectangle
			to.drawBorder(cell->pos, Colors::BRIGHT_YELLOW, 2);
		}
	}
	
	// Draw focus indicator for hero cards
	if (navMode == BEST_HEROES && currentHeroIndex < bestHeroCards.size())
	{
		auto card = bestHeroCards[currentHeroIndex];
		if (card && card->hasFocus())
		{
			to.drawBorder(card->pos, Colors::BRIGHT_YELLOW, 2);
		}
	}
	
	// Draw focus indicator for personality cards
	if (navMode == PERSONALITIES && currentPersonalityIndex < personalityCards.size())
	{
		auto card = personalityCards[currentPersonalityIndex];
		if (card && card->hasFocus())
		{
			to.drawBorder(card->pos, Colors::BRIGHT_YELLOW, 2);
		}
	}
}