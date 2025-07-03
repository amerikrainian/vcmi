/*
 * CSpellWindow.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "CSpellWindow.h"

#include "../../lib/ScopeGuard.h"

#include "GUIClasses.h"
#include "InfoWindows.h"
#include "CCastleInterface.h"

#include "../CPlayerInterface.h"
#include "../PlayerLocalState.h"

#include "../battle/BattleInterface.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/Shortcut.h"
#include "../gui/WindowHandler.h"
#include "../gui/AccessibilityManager.h"
#include "../media/IVideoPlayer.h"
#include "../widgets/GraphicalPrimitiveCanvas.h"
#include "../widgets/CComponent.h"
#include "../widgets/CTextInput.h"
#include "../widgets/TextControls.h"
#include "../widgets/Buttons.h"
#include "../widgets/VideoWidget.h"
#include "../adventureMap/AdventureMapInterface.h"
#include "../render/Canvas.h"
#include "../render/Colors.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/GameConstants.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/battle/CPlayerBattleCallback.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/spells/CSpellHandler.h"
#include "../../lib/spells/ISpellMechanics.h"
#include "../../lib/spells/Problem.h"
#include "../../lib/spells/SpellSchoolHandler.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/texts/TextOperations.h"
#include "../../lib/mapObjects/CGHeroInstance.h"

// Ordering of spell school tabs in SpelTab.def
static const std::array schoolTabOrder =
{
	SpellSchool::AIR,
	SpellSchool::FIRE,
	SpellSchool::WATER,
	SpellSchool::EARTH,
	SpellSchool::ANY
};

CSpellWindow::InteractiveArea::InteractiveArea(const Rect & myRect, std::function<void()> funcL, int helpTextId, CSpellWindow * _owner)
{
	addUsedEvents(LCLICK | SHOW_POPUP | HOVER);
	pos = myRect;
	onLeft = funcL;
	hoverText = LIBRARY->generaltexth->zelp[helpTextId].first;
	helpText = LIBRARY->generaltexth->zelp[helpTextId].second;
	owner = _owner;
}

void CSpellWindow::InteractiveArea::clickPressed(const Point & cursorPosition)
{
	onLeft();
}

void CSpellWindow::InteractiveArea::showPopupWindow(const Point & cursorPosition)
{
	CRClickPopup::createAndPush(helpText);
}

void CSpellWindow::InteractiveArea::hover(bool on)
{
	if(on)
		owner->statusBar->write(hoverText);
	else
		owner->statusBar->clear();
}

class SpellbookSpellSorter
{
public:
	bool operator()(const CSpell * A, const CSpell * B)
	{
		if(A->getLevel() < B->getLevel())
			return true;
		if(A->getLevel() > B->getLevel())
			return false;

		for (const auto schoolId : LIBRARY->spellSchoolHandler->getAllObjects())
		{
			if(A->schools.count(schoolId) && !B->schools.count(schoolId))
				return true;
			if(!A->schools.count(schoolId) && B->schools.count(schoolId))
				return false;
		}

		return TextOperations::compareLocalizedStrings(A->getNameTranslated(), B->getNameTranslated());
	}
};

CSpellWindow::CSpellWindow(const CGHeroInstance * _myHero, CPlayerInterface * _myInt, bool openOnBattleSpells, const std::function<void(SpellID)> & onSpellSelect):
	CWindowObject(PLAYER_COLORED | (settings["gameTweaks"]["enableLargeSpellbook"].Bool() ? BORDERED : 0)),
	battleSpellsOnly(openOnBattleSpells),
	selectedTab(4),
	currentPage(0),
	myHero(_myHero),
	myInt(_myInt),
	openOnBattleSpells(openOnBattleSpells),
	onSpellSelect(onSpellSelect),
	isBigSpellbook(settings["gameTweaks"]["enableLargeSpellbook"].Bool()),
	spellsPerPage(24),
	offL(-11),
	offR(195),
	offRM(110),
	offT(-37),
	offB(56)
{
	OBJECT_CONSTRUCTION;

	if(isBigSpellbook)
	{
		background = std::make_shared<CPicture>(ImagePath::builtin("SpellBookLarge"), 0, 0);
		updateShadow();
	}
	else
	{
		background = std::make_shared<CPicture>(ImagePath::builtin("SpelBack"), 0, 0);
		offL = offR = offT = offB = offRM = 0;
		spellsPerPage = 12;
	}

	pos = background->center(Point(pos.w/2 + pos.x, pos.h/2 + pos.y));

	Rect r(90, isBigSpellbook ? 480 : 420, isBigSpellbook ? 160 : 110, 16);
	if(settings["general"]["enableUiEnhancements"].Bool())
	{
		const ColorRGBA rectangleColor = ColorRGBA(0, 0, 0, 75);
		const ColorRGBA borderColor = ColorRGBA(128, 100, 75);
		const ColorRGBA grayedColor = ColorRGBA(158, 130, 105);
		searchBoxRectangle = std::make_shared<TransparentFilledRectangle>(r.resize(1), rectangleColor, borderColor);
		searchBoxDescription = std::make_shared<CLabel>(r.center().x, r.center().y, FONT_SMALL, ETextAlignment::CENTER, grayedColor, LIBRARY->generaltexth->translate("vcmi.spellBook.search"));

		searchBox = std::make_shared<CTextInput>(r, FONT_SMALL, ETextAlignment::CENTER, false);
		searchBox->setCallback(std::bind(&CSpellWindow::searchInput, this));
	}

	if(onSpellSelect)
	{
		Point boxPos = r.bottomLeft() + Point(-2, 5);
		showAllSpells = std::make_shared<CToggleButton>(boxPos, AnimationPath::builtin("sysopchk.def"), CButton::tooltip(LIBRARY->generaltexth->translate("core.help.458.hover"), LIBRARY->generaltexth->translate("core.help.458.hover")), [this](bool state){ searchInput(); });
		showAllSpellsDescription = std::make_shared<CLabel>(boxPos.x + 40, boxPos.y + 12, FONT_SMALL, ETextAlignment::CENTERLEFT, Colors::WHITE, LIBRARY->generaltexth->translate("core.help.458.hover"));
	}

	processSpells();

	//numbers of spell pages computed

	leftCorner = std::make_shared<CPicture>(ImagePath::builtin("SpelTrnL.bmp"), 97 + offL, 77 + offT);
	rightCorner = std::make_shared<CPicture>(ImagePath::builtin("SpelTrnR.bmp"), 487 + offR, 72 + offT);

	schoolTab = std::make_shared<CAnimImage>(AnimationPath::builtin("SpelTab"), selectedTab, 0, 524 + offR, 88);
	schoolPicture = std::make_shared<CAnimImage>(AnimationPath::builtin("Schools"), 0, 0, 117 + offL, 74 + offT);

	mana = std::make_shared<CLabel>(435 + (isBigSpellbook ? 159 : 0), 426 + offB, FONT_SMALL, ETextAlignment::CENTER, Colors::YELLOW, std::to_string(myHero->mana));

	if(isBigSpellbook)
		statusBar = CGStatusBar::create(400, 587);
	else
		statusBar = CGStatusBar::create(7, 569, ImagePath::builtin("Spelroll.bmp"));

	Rect schoolRect( 549 + pos.x + offR, 94 + pos.y, 45, 35);
	
	// Create interactive areas with accessibility info
	auto exitArea = std::make_shared<InteractiveArea>( Rect( 479 + pos.x + (isBigSpellbook ? 175 : 0), 405 + pos.y + offB, isBigSpellbook ? 60 : 36, 56), std::bind(&CSpellWindow::fexitb, this), 460, this);
	exitArea->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Exit")
		.withDescription("Close spellbook")
		.withTabOrder(10));
	interactiveAreas.push_back(exitArea);
	
	auto battleArea = std::make_shared<InteractiveArea>( Rect( 221 + pos.x + (isBigSpellbook ? 43 : 0), 405 + pos.y + offB, isBigSpellbook ? 60 : 36, 56), std::bind(&CSpellWindow::fbattleSpellsb, this), 453, this);
	battleArea->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Battle spells")
		.withDescription("Show combat spells")
		.withTabOrder(11));
	interactiveAreas.push_back(battleArea);
	
	auto advArea = std::make_shared<InteractiveArea>( Rect( 355 + pos.x + (isBigSpellbook ? 110 : 0), 405 + pos.y + offB, isBigSpellbook ? 60 : 36, 56), std::bind(&CSpellWindow::fadvSpellsb, this), 452, this);
	advArea->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Adventure spells")
		.withDescription("Show adventure map spells")
		.withTabOrder(12));
	interactiveAreas.push_back(advArea);
	
	auto manaArea = std::make_shared<InteractiveArea>( Rect( 418 + pos.x + (isBigSpellbook ? 142 : 0), 405 + pos.y + offB, isBigSpellbook ? 60 : 36, 56), std::bind(&CSpellWindow::fmanaPtsb, this), 459, this);
	manaArea->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Mana points")
		.withDescription("Show mana information")
		.withTabOrder(13));
	interactiveAreas.push_back(manaArea);
	
	// School selection areas
	const std::string schoolNames[] = {"Air magic", "Earth magic", "Fire magic", "Water magic", "All schools"};
	const int schoolOrder[] = {0, 3, 1, 2, 4};
	for(int i = 0; i < 5; ++i)
	{
		auto schoolArea = std::make_shared<InteractiveArea>( schoolRect + Point(0, i * (i < 4 ? 57 : 60) + (i == 1 ? 2 : 0) + (i >= 2 ? 2 : 0)), std::bind(&CSpellWindow::selectSchool, this, schoolOrder[i]), 454 + schoolOrder[i], this);
		schoolArea->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("button")
			.withName(schoolNames[schoolOrder[i]])
			.withDescription("Select " + schoolNames[schoolOrder[i]] + " school")
			.withTabOrder(20 + i));
		interactiveAreas.push_back(schoolArea);
	}
	
	auto leftArea = std::make_shared<InteractiveArea>( Rect(  97 + offL + pos.x, 77 + offT + pos.y, leftCorner->pos.h,  leftCorner->pos.w  ), std::bind(&CSpellWindow::fLcornerb, this), 450, this);
	leftArea->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Previous page")
		.withDescription("Turn to previous page")
		.withTabOrder(30));
	interactiveAreas.push_back(leftArea);
	
	auto rightArea = std::make_shared<InteractiveArea>( Rect( 487 + offR + pos.x, 72 + offT + pos.y, rightCorner->pos.h, rightCorner->pos.w ), std::bind(&CSpellWindow::fRcornerb, this), 451, this);
	rightArea->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Next page")
		.withDescription("Turn to next page")
		.withTabOrder(31));
	interactiveAreas.push_back(rightArea);

	//areas for spells
	int xpos = 117 + offL + pos.x;
	int ypos = 90 + offT + pos.y;

	for(int v=0; v<spellsPerPage; ++v)
	{
		spellAreas[v] = std::make_shared<SpellArea>( Rect(xpos, ypos, 65, 78), this);

		if(v == (spellsPerPage / 2) - 1) //to right page
		{
			xpos = offRM + 336 + pos.x; ypos = 90 + offT + pos.y;
		}
		else
		{
			if(v%(isBigSpellbook ? 3 : 2) == 0 || (v%3 == 1 && isBigSpellbook))
			{
				xpos+=85;
			}
			else
			{
				xpos -= (isBigSpellbook ? 2 : 1)*85; ypos+=97;
			}
		}
	}

	selectedTab = battleSpellsOnly ? myInt->localState->getSpellbookSettings().spellbookLastTabBattle : myInt->localState->getSpellbookSettings().spellbookLastTabAdvmap;
	schoolTab->setFrame(selectedTab, 0);
	int cp = battleSpellsOnly ? myInt->localState->getSpellbookSettings().spellbookLastPageBattle : myInt->localState->getSpellbookSettings().spellbookLastPageAdvmap;
	// spellbook last page battle index is not reset after battle, so this needs to stay here
	vstd::abetween(cp, 0, std::max(0, pagesWithinCurrentTab() - 1));
	setCurrentPage(cp);
	computeSpellsPerArea();
	addUsedEvents(KEYBOARD);
	
	// Add accessibility info for spell window
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("window")
		.withName("Spellbook")
		.withDescription("Hero spellbook. Use arrow keys to navigate spells, change pages and schools. Press Enter to cast spell.")
		.withTabOrder(0));
	
	// Set initial focus to first available spell
	currentSpellIndex = 0;
	for(int i = 0; i < spellsPerPage; ++i)
	{
		if(spellAreas[i]->mySpell)
		{
			setFocusedSpell(i);
			break;
		}
	}
	
	// Announce window opening
	std::string announcement = "Spellbook opened. ";
	announcement += std::to_string(mySpells.size()) + " spells available. ";
	announcement += "Current school: " + (selectedTab == 4 ? "All schools" : LIBRARY->spellSchoolHandler->getById(schoolTabOrder[selectedTab])->getJsonKey());
	AccessibilityManager::getInstance().announce(announcement, true);
}

CSpellWindow::~CSpellWindow()
{
}

void CSpellWindow::searchInput()
{
	if(searchBox)
		searchBoxDescription->setEnabled(searchBox->getText().empty());

	processSpells();

	int cp = 0;
	// spellbook last page battle index is not reset after battle, so this needs to stay here
	vstd::abetween(cp, 0, std::max(0, pagesWithinCurrentTab() - 1));
	setCurrentPage(cp);
	computeSpellsPerArea();
}

void CSpellWindow::processSpells()
{
	mySpells.clear();

	//initializing castable spells
	mySpells.reserve(LIBRARY->spellh->objects.size());
	for(auto const & spell : LIBRARY->spellh->objects)
	{
		bool searchTextFound = !searchBox || TextOperations::textSearchSimilarityScore(searchBox->getText(), spell->getNameTranslated());

		if(onSpellSelect)
		{
			if(spell->isCombat() == openOnBattleSpells
				&& !spell->isSpecial()
				&& !spell->isCreatureAbility()
				&& searchTextFound
				&& (showAllSpells->isSelected() || myHero->canCastThisSpell(spell.get())))
			{
				mySpells.push_back(spell.get());
			}
			continue;
		}

		if(!spell->isCreatureAbility() && myHero->canCastThisSpell(spell.get()) && searchTextFound)
			mySpells.push_back(spell.get());
	}

	SpellbookSpellSorter spellsorter;
	std::sort(mySpells.begin(), mySpells.end(), spellsorter);

	//initializing sizes of spellbook's parts
	for(auto & elem : sitesPerTabAdv)
		elem = 0;
	for(auto & elem : sitesPerTabBattle)
		elem = 0;

	for(const auto spell : mySpells)
	{
		int * sitesPerOurTab = spell->isCombat() ? sitesPerTabBattle : sitesPerTabAdv;

		++sitesPerOurTab[4];

		spell->forEachSchool([&sitesPerOurTab](const SpellSchool & school, bool & stop)
		{
			++sitesPerOurTab[school.getNum()];
		});
	}
	if(sitesPerTabAdv[4] % spellsPerPage == 0)
		sitesPerTabAdv[4]/=spellsPerPage;
	else
		sitesPerTabAdv[4] = sitesPerTabAdv[4]/spellsPerPage + 1;

	for(int v=0; v<4; ++v)
	{
		if(sitesPerTabAdv[v] <= spellsPerPage - 2)
			sitesPerTabAdv[v] = 1;
		else
		{
			if((sitesPerTabAdv[v] - (spellsPerPage - 2)) % spellsPerPage == 0)
				sitesPerTabAdv[v] = (sitesPerTabAdv[v] - (spellsPerPage - 2)) / spellsPerPage + 1;
			else
				sitesPerTabAdv[v] = (sitesPerTabAdv[v] - (spellsPerPage - 2)) / spellsPerPage + 2;
		}
	}

	if(sitesPerTabBattle[4] % spellsPerPage == 0)
		sitesPerTabBattle[4]/=spellsPerPage;
	else
		sitesPerTabBattle[4] = sitesPerTabBattle[4]/spellsPerPage + 1;

	for(int v=0; v<4; ++v)
	{
		if(sitesPerTabBattle[v] <= spellsPerPage - 2)
			sitesPerTabBattle[v] = 1;
		else
		{
			if((sitesPerTabBattle[v] - (spellsPerPage - 2)) % spellsPerPage == 0)
				sitesPerTabBattle[v] = (sitesPerTabBattle[v] - (spellsPerPage - 2)) / spellsPerPage + 1;
			else
				sitesPerTabBattle[v] = (sitesPerTabBattle[v] - (spellsPerPage - 2)) / spellsPerPage + 2;
		}
	}
}

void CSpellWindow::fexitb()
{
	auto spellBookState = myInt->localState->getSpellbookSettings();
	if(myInt->battleInt)
	{
		spellBookState.spellbookLastTabBattle = selectedTab;
		spellBookState.spellbookLastPageBattle = currentPage;
	}
	else
	{
		spellBookState.spellbookLastTabAdvmap = selectedTab;
		spellBookState.spellbookLastPageAdvmap = currentPage;
	}
	myInt->localState->setSpellbookSettings(spellBookState);

	if(onSpellSelect)
		onSpellSelect(SpellID::NONE);

	close();
}

void CSpellWindow::fadvSpellsb()
{
	if(battleSpellsOnly == true)
	{
		turnPageRight();
		battleSpellsOnly = false;
		setCurrentPage(0);
	}
	computeSpellsPerArea();
}

void CSpellWindow::fbattleSpellsb()
{
	if(battleSpellsOnly == false)
	{
		turnPageLeft();
		battleSpellsOnly = true;
		setCurrentPage(0);
	}
	computeSpellsPerArea();
}

void CSpellWindow::toggleSearchBoxFocus()
{
	if(searchBox != nullptr)
	{
		searchBox->hasFocus() ? searchBox->removeFocus() : searchBox->giveFocus();
	}
}

void CSpellWindow::fmanaPtsb()
{
}

void CSpellWindow::selectSchool(int school)
{
	if(selectedTab != school)
	{
		if(selectedTab < school)
			turnPageLeft();
		else
			turnPageRight();
		selectedTab = school;
		schoolTab->setFrame(selectedTab, 0);
		setCurrentPage(0);
	}
	computeSpellsPerArea();
	
	// Set focus to first available spell in new school
	for(int i = 0; i < spellsPerPage; ++i)
	{
		if(spellAreas[i]->mySpell)
		{
			setFocusedSpell(i);
			break;
		}
	}
	
	// Announce school change
	std::string schoolName = school == 4 ? "All schools" : LIBRARY->spellSchoolHandler->getById(schoolTabOrder[school])->getJsonKey();
	AccessibilityManager::getInstance().announce("Switched to " + schoolName + " school. " + std::to_string(pagesWithinCurrentTab()) + " pages available.");
}

void CSpellWindow::fLcornerb()
{
	if(currentPage>0)
	{
		turnPageLeft();
		setCurrentPage(currentPage - 1);
	}
	computeSpellsPerArea();
	
	// Set focus to first available spell on new page
	for(int i = 0; i < spellsPerPage; ++i)
	{
		if(spellAreas[i]->mySpell)
		{
			setFocusedSpell(i);
			break;
		}
	}
	
	AccessibilityManager::getInstance().announce("Previous page. Page " + std::to_string(currentPage + 1) + " of " + std::to_string(pagesWithinCurrentTab()));
}

void CSpellWindow::fRcornerb()
{
	if((currentPage + 1) < (pagesWithinCurrentTab()))
	{
		turnPageRight();
		setCurrentPage(currentPage + 1);
	}
	computeSpellsPerArea();
	
	// Set focus to first available spell on new page
	for(int i = 0; i < spellsPerPage; ++i)
	{
		if(spellAreas[i]->mySpell)
		{
			setFocusedSpell(i);
			break;
		}
	}
	
	AccessibilityManager::getInstance().announce("Next page. Page " + std::to_string(currentPage + 1) + " of " + std::to_string(pagesWithinCurrentTab()));
}

void CSpellWindow::show(Canvas & to)
{
	if(video)
		video->show(to);
	statusBar->show(to);
	
	// Draw focus indicator for the current spell
	if(currentSpellIndex >= 0 && currentSpellIndex < spellsPerPage && spellAreas[currentSpellIndex])
	{
		auto focusedArea = spellAreas[currentSpellIndex];
		if(focusedArea->mySpell)
		{
			// Draw yellow border around focused spell
			to.drawBorder(focusedArea->pos, Colors::BRIGHT_YELLOW, 2);
		}
	}
}

void CSpellWindow::computeSpellsPerArea()
{
	std::vector<const CSpell *> spellsCurSite;
	spellsCurSite.reserve(mySpells.size());
	for(const CSpell * spell : mySpells)
	{
		if(spell->isCombat() ^ !battleSpellsOnly
		   && ((selectedTab == 4) || spell->schools.count(schoolTabOrder.at(selectedTab)))
			)
		{
			spellsCurSite.push_back(spell);
		}
	}

	if(selectedTab == 4)
	{
		if(spellsCurSite.size() > spellsPerPage)
		{
			spellsCurSite = std::vector<const CSpell *>(spellsCurSite.begin() + currentPage*spellsPerPage, spellsCurSite.end());
			if(spellsCurSite.size() > spellsPerPage)
			{
				spellsCurSite.erase(spellsCurSite.begin()+spellsPerPage, spellsCurSite.end());
			}
		}
	}
	else //selectedTab == 0, 1, 2 or 3
	{
		if(spellsCurSite.size() > spellsPerPage - 2)
		{
			if(currentPage == 0)
			{
				spellsCurSite.erase(spellsCurSite.begin()+spellsPerPage-2, spellsCurSite.end());
			}
			else
			{
				spellsCurSite = std::vector<const CSpell *>(spellsCurSite.begin() + (currentPage-1)*spellsPerPage + spellsPerPage-2, spellsCurSite.end());
				if(spellsCurSite.size() > spellsPerPage)
				{
					spellsCurSite.erase(spellsCurSite.begin()+spellsPerPage, spellsCurSite.end());
				}
			}
		}
	}
	//applying
	if(selectedTab == 4 || currentPage != 0)
	{
		for(size_t c=0; c<spellsPerPage; ++c)
		{
			if(c < spellsCurSite.size())
			{
				spellAreas[c]->setSpell(spellsCurSite[c]);
			}
			else
			{
				spellAreas[c]->setSpell(nullptr);
			}
		}
	}
	else
	{
		spellAreas[0]->setSpell(nullptr);
		spellAreas[1]->setSpell(nullptr);
		for(size_t c=0; c<spellsPerPage-2; ++c)
		{
			if(c < spellsCurSite.size())
				spellAreas[c+2]->setSpell(spellsCurSite[c]);
			else
				spellAreas[c+2]->setSpell(nullptr);
		}
	}
	redraw();
}

void CSpellWindow::setCurrentPage(int value)
{
	currentPage = value;
	schoolPicture->visible = selectedTab!=4 && currentPage == 0;
	if(selectedTab != 4)
		schoolPicture->setFrame(selectedTab, 0);

	if (currentPage != 0)
		leftCorner->enable();
	else
		leftCorner->disable();

	if (currentPage + 1 < pagesWithinCurrentTab())
		rightCorner->enable();
	else
		rightCorner->disable();

	mana->setText(std::to_string(myHero->mana));//just in case, it will be possible to cast spell without closing book
}

void CSpellWindow::turnPageLeft()
{
	OBJECT_CONSTRUCTION;
	if(settings["video"]["spellbookAnimation"].Bool() && !isBigSpellbook)
		video = std::make_shared<VideoWidgetOnce>(Point(13, 14), VideoPath::builtin("PGTRNLFT.SMK"), false, this);
}

void CSpellWindow::turnPageRight()
{
	OBJECT_CONSTRUCTION;
	if(settings["video"]["spellbookAnimation"].Bool() && !isBigSpellbook)
		video = std::make_shared<VideoWidgetOnce>(Point(13, 14), VideoPath::builtin("PGTRNRGH.SMK"), false, this);
}

void CSpellWindow::onVideoPlaybackFinished()
{
	video.reset();
	redraw();
}

void CSpellWindow::keyPressed(EShortcut key)
{
	switch(key)
	{
		case EShortcut::GLOBAL_RETURN:
			fexitb();
			break;

		case EShortcut::MOVE_LEFT:
			if(getFocusedCol() > 0)
			{
				// Navigate within spell grid
				navigateToSpell(getFocusedRow(), getFocusedCol() - 1);
			}
			else if(currentSpellIndex >= spellsPerPage / 2)
			{
				// Jump to left page from right page
				navigateToSpell(getFocusedRow(), getSpellsPerRow() - 1);
			}
			else
			{
				// At leftmost position, turn page
				fLcornerb();
			}
			break;
			
		case EShortcut::MOVE_RIGHT:
			if(getFocusedCol() < getSpellsPerRow() - 1 && 
			   (currentSpellIndex < spellsPerPage / 2 - 1 || 
			    (currentSpellIndex >= spellsPerPage / 2 && currentSpellIndex < spellsPerPage - 1)))
			{
				// Navigate within spell grid
				navigateToSpell(getFocusedRow(), getFocusedCol() + 1);
			}
			else if(currentSpellIndex < spellsPerPage / 2)
			{
				// Jump to right page from left page
				navigateToSpell(getFocusedRow(), 0);
			}
			else
			{
				// At rightmost position, turn page
				fRcornerb();
			}
			break;
			
		case EShortcut::MOVE_UP:
			if(getFocusedRow() > 0)
			{
				// Navigate up within grid
				navigateToSpell(getFocusedRow() - 1, getFocusedCol());
			}
			else
			{
				// At top row, switch schools up
				static const int schoolsOrder[] = { 0, 3, 1, 2, 4 };
				int index = -1;
				while(schoolsOrder[++index] != selectedTab);
				index--;
				if(index >= 0)
					selectSchool(schoolsOrder[index]);
			}
			break;
			
		case EShortcut::MOVE_DOWN:
			if(getFocusedRow() < getSpellsPerColumn() - 1)
			{
				// Navigate down within grid
				navigateToSpell(getFocusedRow() + 1, getFocusedCol());
			}
			else
			{
				// At bottom row, switch schools down
				static const int schoolsOrder[] = { 0, 3, 1, 2, 4 };
				int index = -1;
				while(schoolsOrder[++index] != selectedTab);
				index++;
				if(index < std::size(schoolsOrder))
					selectSchool(schoolsOrder[index]);
			}
			break;
			
		case EShortcut::GLOBAL_ACCEPT:
			// Activate current spell
			if(currentSpellIndex < spellsPerPage && spellAreas[currentSpellIndex]->mySpell)
			{
				spellAreas[currentSpellIndex]->clickPressed(Point());
			}
			break;
			
		case EShortcut::SPELLBOOK_TAB_COMBAT:
			fbattleSpellsb();
			break;
		case EShortcut::SPELLBOOK_TAB_ADVENTURE:
			fadvSpellsb();
			break;
		case EShortcut::SPELLBOOK_SEARCH_FOCUS:
			toggleSearchBoxFocus();
			break;
			
		// Number keys for quick spell selection
		case EShortcut::SELECT_INDEX_1:
		case EShortcut::SELECT_INDEX_2:
		case EShortcut::SELECT_INDEX_3:
		case EShortcut::SELECT_INDEX_4:
		case EShortcut::SELECT_INDEX_5:
		case EShortcut::SELECT_INDEX_6:
		case EShortcut::SELECT_INDEX_7:
		case EShortcut::SELECT_INDEX_8:
		{
			int index = static_cast<int>(key) - static_cast<int>(EShortcut::SELECT_INDEX_1);
			if(index < spellsPerPage && spellAreas[index]->mySpell)
			{
				setFocusedSpell(index);
			}
			break;
		}
			
		// Page navigation
		case EShortcut::MOVE_PAGE_UP:
			fLcornerb();
			break;
		case EShortcut::MOVE_PAGE_DOWN:
			fRcornerb();
			break;
			
		default:
			CWindowObject::keyPressed(key);
			break;
	}
}

int CSpellWindow::pagesWithinCurrentTab()
{
	return battleSpellsOnly ? sitesPerTabBattle[selectedTab] : sitesPerTabAdv[selectedTab];
}

void CSpellWindow::navigateSpells(int delta)
{
	int newIndex = currentSpellIndex + delta;
	if(newIndex >= 0 && newIndex < spellsPerPage)
	{
		setFocusedSpell(newIndex);
	}
}

void CSpellWindow::setFocusedSpell(int index)
{
	if(index >= 0 && index < spellsPerPage)
	{
		currentSpellIndex = index;
		
		// Update focus for all spell areas
		for(int i = 0; i < spellsPerPage; ++i)
		{
			if(spellAreas[i])
			{
				spellAreas[i]->setFocus(i == currentSpellIndex);
			}
		}
		
		// Announce the newly focused spell
		announceCurrentSpell();
		redraw();
	}
}

void CSpellWindow::announceCurrentSpell()
{
	if(currentSpellIndex < spellsPerPage && spellAreas[currentSpellIndex])
	{
		auto spellArea = spellAreas[currentSpellIndex];
		if(spellArea->mySpell)
		{
			std::string announcement = spellArea->mySpell->getNameTranslated();
			announcement += ", Level " + std::to_string(spellArea->mySpell->getLevel());
			announcement += ", Cost: " + std::to_string(myInt->cb->getSpellCost(spellArea->mySpell, myHero));
			
			if(myInt->cb->getSpellCost(spellArea->mySpell, myHero) > myHero->mana && !onSpellSelect)
			{
				announcement += " (Insufficient mana)";
			}
			
			// Add position info
			int row = getFocusedRow() + 1;
			int col = getFocusedCol() + 1;
			int page = currentSpellIndex < spellsPerPage / 2 ? 1 : 2;
			announcement += ". Row " + std::to_string(row) + ", Column " + std::to_string(col) + ", Page " + std::to_string(page);
			
			AccessibilityManager::getInstance().announce(announcement);
		}
		else
		{
			AccessibilityManager::getInstance().announce("Empty spell slot");
		}
	}
}

void CSpellWindow::navigateToSpell(int row, int col)
{
	// Calculate the spell index based on which page we're on
	int baseIndex = currentSpellIndex < spellsPerPage / 2 ? 0 : spellsPerPage / 2;
	int newIndex = baseIndex + row * getSpellsPerRow() + col;
	
	// Ensure we don't go out of bounds
	if(newIndex >= baseIndex && newIndex < baseIndex + spellsPerPage / 2 && newIndex < spellsPerPage)
	{
		setFocusedSpell(newIndex);
	}
}

CSpellWindow::SpellArea::SpellArea(Rect pos, CSpellWindow * owner)
{
	this->pos = pos;
	this->owner = owner;
	addUsedEvents(LCLICK | SHOW_POPUP | HOVER | KEYBOARD);

	schoolLevel = -1;
	mySpell = nullptr;

	OBJECT_CONSTRUCTION;

	image = std::make_shared<CAnimImage>(AnimationPath::builtin("Spells"), 0, 0);
	image->visible = false;

	name = std::make_shared<CLabel>(39, 70, FONT_TINY, ETextAlignment::CENTER);
	level = std::make_shared<CLabel>(39, 82, FONT_TINY, ETextAlignment::CENTER);
	cost = std::make_shared<CLabel>(39, 94, FONT_TINY, ETextAlignment::CENTER);

	for(auto l : {name, level, cost})
		l->setAutoRedraw(false);
}

CSpellWindow::SpellArea::~SpellArea() = default;

void CSpellWindow::SpellArea::clickPressed(const Point & cursorPosition)
{
	if(mySpell)
	{
		if(owner->onSpellSelect)
		{
			owner->onSpellSelect(mySpell->id);
			owner->close();
			return;
		}

		auto spellCost = owner->myInt->cb->getSpellCost(mySpell, owner->myHero);
		if(spellCost > owner->myHero->mana) //insufficient mana
		{
			GAME->interface()->showInfoDialog(boost::str(boost::format(LIBRARY->generaltexth->allTexts[206]) % spellCost % owner->myHero->mana));
			return;
		}

		//anything that is not combat spell is adventure spell
		//this not an error in general to cast even creature ability with hero
		const bool combatSpell = mySpell->isCombat();
		if(combatSpell == mySpell->isAdventure())
		{
			logGlobal->error("Spell have invalid flags");
			return;
		}

		const bool inCombat = owner->myInt->battleInt != nullptr;
		const bool inCastle = owner->myInt->castleInt != nullptr;

		//battle spell on adv map or adventure map spell during combat => display infowindow, not cast
		if((combatSpell != inCombat) || inCastle || (!combatSpell && !GAME->interface()->makingTurn))
		{
			std::vector<std::shared_ptr<CComponent>> hlp(1, std::make_shared<CComponent>(ComponentType::SPELL, mySpell->id));
			GAME->interface()->showInfoDialog(mySpell->getDescriptionTranslated(schoolLevel), hlp);
		}
		else if(combatSpell)
		{
			spells::detail::ProblemImpl problem;
			if(mySpell->canBeCast(problem, owner->myInt->battleInt->getBattle().get(), spells::Mode::HERO, owner->myHero))
			{
				owner->myInt->battleInt->castThisSpell(mySpell->id);
				owner->fexitb();
			}
			else
			{
				std::vector<std::string> texts;
				problem.getAll(texts);
				if(!texts.empty())
					GAME->interface()->showInfoDialog(texts.front());
				else
					GAME->interface()->showInfoDialog(LIBRARY->generaltexth->translate("vcmi.adventureMap.spellUnknownProblem"));
			}
		}
		else //adventure spell
		{
			const CGHeroInstance * h = owner->myHero;
			ENGINE->windows().popWindows(1);

			auto guard = vstd::makeScopeGuard([this]()
			{
				auto spellBookState = owner->myInt->localState->getSpellbookSettings();
				spellBookState.spellbookLastTabAdvmap = owner->selectedTab;
				spellBookState.spellbookLastPageAdvmap = owner->currentPage;
				owner->myInt->localState->setSpellbookSettings(spellBookState);
			});

			spells::detail::ProblemImpl problem;
			if (mySpell->getAdventureMechanics().canBeCast(problem, GAME->interface()->cb.get(), owner->myHero))
			{
				if(mySpell->getTargetType() == spells::AimType::LOCATION)
					adventureInt->enterCastingMode(mySpell);
				else if(mySpell->getTargetType() == spells::AimType::NO_TARGET)
					owner->myInt->cb->castSpell(h, mySpell->id);
				else
					logGlobal->error("Invalid spell target type");
			}
			else
			{
				std::vector<std::string> texts;
				problem.getAll(texts);
				if(!texts.empty())
					GAME->interface()->showInfoDialog(texts.front());
				else
					GAME->interface()->showInfoDialog(LIBRARY->generaltexth->translate("vcmi.adventureMap.spellUnknownProblem"));
			}
		}
	}
}

void CSpellWindow::SpellArea::showPopupWindow(const Point & cursorPosition)
{
	if(mySpell)
	{
		std::string dmgInfo;
		auto causedDmg = owner->myInt->cb->estimateSpellDamage(mySpell, owner->myHero);
		if(causedDmg == 0 || mySpell->id == SpellID::TITANS_LIGHTNING_BOLT) //Titan's Lightning Bolt already has damage info included
			dmgInfo.clear();
		else
		{
			dmgInfo = LIBRARY->generaltexth->allTexts[343];
			boost::algorithm::replace_first(dmgInfo, "%d", std::to_string(causedDmg));
		}

		CRClickPopup::createAndPush(mySpell->getDescriptionTranslated(schoolLevel) + dmgInfo, std::make_shared<CComponent>(ComponentType::SPELL, mySpell->id));
	}
}

void CSpellWindow::SpellArea::hover(bool on)
{
	if(mySpell)
	{
		if(on)
			owner->statusBar->write(boost::str(boost::format("%s (%s)") % mySpell->getNameTranslated() % LIBRARY->generaltexth->allTexts[171+mySpell->getLevel()]));
		else
			owner->statusBar->clear();
	}
}

void CSpellWindow::SpellArea::keyPressed(EShortcut key)
{
	if(key == EShortcut::GLOBAL_ACCEPT && mySpell)
	{
		clickPressed(Point());
	}
}

void CSpellWindow::SpellArea::setSpell(const CSpell * spell)
{
	schoolBorder.reset();
	image->visible = false;
	name->setText("");
	level->setText("");
	cost->setText("");
	mySpell = spell;
	if(mySpell)
	{
		SpellSchool whichSchool;
		schoolLevel = owner->myHero->getSpellSchoolLevel(mySpell, &whichSchool);
		auto spellCost = owner->myInt->cb->getSpellCost(mySpell, owner->myHero);

		image->setFrame(mySpell->id.getNum());
		image->visible = true;

		{
			OBJECT_CONSTRUCTION;

			schoolBorder.reset();
			if (owner->selectedTab >= 4)
			{
				if (whichSchool.hasValue())
					schoolBorder = std::make_shared<CAnimImage>(LIBRARY->spellSchoolHandler->getById(whichSchool)->getSpellBordersPath(), schoolLevel);
			}
			else
				schoolBorder = std::make_shared<CAnimImage>(LIBRARY->spellSchoolHandler->getById(schoolTabOrder.at(owner->selectedTab))->getSpellBordersPath(), schoolLevel);
		}

		ColorRGBA firstLineColor, secondLineColor;
		if(spellCost > owner->myHero->mana && !owner->onSpellSelect) //hero cannot cast this spell
		{
			firstLineColor = Colors::WHITE;
			secondLineColor = Colors::ORANGE;
		}
		else
		{
			firstLineColor = Colors::YELLOW;
			secondLineColor = Colors::WHITE;
		}

		name->color = firstLineColor;
		name->setText(mySpell->getNameTranslated());

		level->color = secondLineColor;
		if(schoolLevel > 0)
		{
			boost::format fmt("%s/%s");
			fmt % LIBRARY->generaltexth->allTexts[171 + mySpell->getLevel()];
			fmt % LIBRARY->generaltexth->levels[3+(schoolLevel-1)];//lines 4-6
			level->setText(fmt.str());
		}
		else
			level->setText(LIBRARY->generaltexth->allTexts[171 + mySpell->getLevel()]);

		cost->color = secondLineColor;
		boost::format costfmt("%s: %d");
		costfmt % LIBRARY->generaltexth->allTexts[387] % spellCost;
		cost->setText(costfmt.str());
		
		// Add accessibility info for spell slots
		std::string spellName = mySpell->getNameTranslated();
		std::string levelText = LIBRARY->generaltexth->allTexts[171 + mySpell->getLevel()];
		std::string fullName = spellName + " - " + levelText;
		std::string description = mySpell->getDescriptionTranslated(schoolLevel) + " Cost: " + std::to_string(spellCost);
		std::string state = (spellCost > owner->myHero->mana && !owner->onSpellSelect) ? "insufficient_mana" : "available";
		
		setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("spell_slot")
			.withName(fullName)
			.withDescription(description)
			.withState(state)
			.withTabOrder(100 + (pos.x / 65) + (pos.y / 97) * 10)); // Calculate tab order based on position
	}
	else
	{
		// Add accessibility info for empty spell slots
		setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("spell_slot")
			.withName("Empty spell slot")
			.withDescription("No spell available in this slot")
			.withState("empty")
			.withTabOrder(100 + (pos.x / 65) + (pos.y / 97) * 10));
	}
}
