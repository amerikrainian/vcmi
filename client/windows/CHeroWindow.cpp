/*
 * CHeroWindow.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "CHeroWindow.h"

#include "CCreatureWindow.h"
#include "CHeroBackpackWindow.h"
#include "CKingdomInterface.h"
#include "CExchangeWindow.h"

#include "../CPlayerInterface.h"

#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/TextAlignment.h"
#include "../gui/Shortcut.h"
#include "../gui/WindowHandler.h"
#include "../gui/AccessibilityManager.h"
#include "../widgets/Images.h"
#include "../widgets/MiscWidgets.h"
#include "../widgets/CComponent.h"
#include "../widgets/CGarrisonInt.h"
#include "../widgets/TextControls.h"
#include "../widgets/Buttons.h"
#include "../widgets/Slider.h"
#include "../render/IRenderHandler.h"

#include "../lib/CConfigHandler.h"
#include "../lib/CSkillHandler.h"
#include "../lib/GameLibrary.h"
#include "../lib/callback/CCallback.h"
#include "../lib/entities/artifact/ArtifactUtils.h"
#include "../lib/entities/hero/CHeroHandler.h"
#include "../lib/mapObjects/CGHeroInstance.h"
#include "../lib/networkPacks/ArtifactLocation.h"
#include "../lib/texts/CGeneralTextHandler.h"

void CHeroSwitcher::clickPressed(const Point & cursorPosition)
{
	//TODO: do not recreate window
	if (false)
	{
		owner->update();
	}
	else
	{
		const CGHeroInstance * buf = hero;
		ENGINE->windows().popWindows(1);
		ENGINE->windows().createAndPushWindow<CHeroWindow>(buf);
	}
}

CHeroSwitcher::CHeroSwitcher(CHeroWindow * owner_, Point pos_, const CGHeroInstance * hero_)
	: CIntObject(LCLICK),
	owner(owner_),
	hero(hero_)
{
	OBJECT_CONSTRUCTION;
	pos += pos_;

	image = std::make_shared<CAnimImage>(AnimationPath::builtin("PortraitsSmall"), hero->getIconIndex());
	pos.w = image->pos.w;
	pos.h = image->pos.h;
	
	// Add accessibility info for hero switcher
	std::string heroName = hero->getNameTranslated();
	std::string heroClass = hero->getClassNameTranslated();
	std::string description = "Switch to " + heroName + ", level " + std::to_string(hero->level) + " " + heroClass;
	
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName(heroName + " - Level " + std::to_string(hero->level))
		.withDescription(description)
		.withTabOrder(80 + (pos_.y - 87) / 54)); // Calculate tab order based on position
	
	addUsedEvents(HOVER | KEYBOARD);
}

CHeroWindow::CHeroWindow(const CGHeroInstance * hero)
	: CWindowObject(PLAYER_COLORED, ImagePath::builtin("HeroScr4"))
{
	auto & heroscrn = LIBRARY->generaltexth->heroscrn;

	OBJECT_CONSTRUCTION;
	curHero = hero;
	
	// Enable keyboard navigation for the window
	addUsedEvents(KEYBOARD);
	
	// Set up accessibility info for the main window
	setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("window")
		.withName(hero->getNameTranslated())
		.withDescription(boost::str(boost::format("Hero window for %s, level %d %s") % hero->getNameTranslated() % hero->level % hero->getClassNameTranslated()))
		.withTabOrder(0));

	banner = std::make_shared<CAnimImage>(AnimationPath::builtin("CREST58"), GAME->interface()->playerID.getNum(), 0, 606, 8);
	name = std::make_shared<CLabel>(190, 38, EFonts::FONT_BIG, ETextAlignment::CENTER, Colors::YELLOW);
	name->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("heading")
		.withName("Hero name")
		.withDescription("Current hero's name")
		.withTabOrder(1));
	
	title = std::make_shared<CLabel>(190, 65, EFonts::FONT_MEDIUM, ETextAlignment::CENTER, Colors::WHITE);
	title->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("heading")
		.withName("Hero level and class")
		.withDescription("Current hero's level and class")
		.withTabOrder(2));

	statusbar = CGStatusBar::create(7, 559, ImagePath::builtin("ADROLLVR.bmp"), 660);
	statusbar->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("status")
		.withName("Status Bar")
		.withDescription("Displays information about various UI elements on hover")
		.withTabOrder(100));
	statusbar->addUsedEvents(KEYBOARD);

	quitButton = std::make_shared<CButton>(Point(609, 516), AnimationPath::builtin("hsbtns.def"), CButton::tooltip(heroscrn[17]), [this](){ close(); }, EShortcut::GLOBAL_RETURN);
	quitButton->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Close")
		.withDescription("Close hero window")
		.withTabOrder(99));

	if(settings["general"]["enableUiEnhancements"].Bool())
	{
		questlogButton = std::make_shared<CButton>(Point(314, 429), AnimationPath::builtin("hsbtns4.def"), CButton::tooltip(heroscrn[0]), [](){ GAME->interface()->showQuestLog(); }, EShortcut::ADVENTURE_QUEST_LOG);
		questlogButton->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("button")
			.withName("Quest Log")
			.withDescription("Open quest log")
			.withTabOrder(90));
		
		backpackButton = std::make_shared<CButton>(Point(424, 429), AnimationPath::builtin("heroBackpack"), CButton::tooltipLocalized("vcmi.heroWindow.openBackpack"), [this](){ createBackpackWindow(); }, EShortcut::HERO_BACKPACK);
		backpackButton->setOverlay(std::make_shared<CPicture>(ImagePath::builtin("heroWindow/backpackButtonIcon")));
		backpackButton->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("button")
			.withName("Backpack")
			.withDescription("Open hero backpack")
			.withTabOrder(91));
		
		dismissButton = std::make_shared<CButton>(Point(534, 429), AnimationPath::builtin("hsbtns2.def"), CButton::tooltip(heroscrn[28]), [this](){ dismissCurrent(); }, EShortcut::HERO_DISMISS);
		dismissButton->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("button")
			.withName("Dismiss")
			.withDescription("Dismiss this hero")
			.withTabOrder(92));
	}
	else
	{
		dismissLabel = std::make_shared<CTextBox>(LIBRARY->generaltexth->jktexts[8], Rect(370, 430, 65, 35), 0, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE);
		dismissLabel->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("text")
			.withName("Dismiss label")
			.withDescription(LIBRARY->generaltexth->jktexts[8])
			.withTabOrder(88));
		questlogLabel = std::make_shared<CTextBox>(LIBRARY->generaltexth->jktexts[9], Rect(510, 430, 65, 35), 0, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE);
		questlogLabel->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("text")
			.withName("Quest log label")
			.withDescription(LIBRARY->generaltexth->jktexts[9])
			.withTabOrder(89));
		
		dismissButton = std::make_shared<CButton>(Point(454, 429), AnimationPath::builtin("hsbtns2.def"), CButton::tooltip(heroscrn[28]), [this](){ dismissCurrent(); }, EShortcut::HERO_DISMISS);
		dismissButton->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("button")
			.withName("Dismiss")
			.withDescription("Dismiss this hero")
			.withTabOrder(92));
		
		questlogButton = std::make_shared<CButton>(Point(314, 429), AnimationPath::builtin("hsbtns4.def"), CButton::tooltip(heroscrn[0]), [](){ GAME->interface()->showQuestLog(); }, EShortcut::ADVENTURE_QUEST_LOG);
		questlogButton->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("button")
			.withName("Quest Log")
			.withDescription("Open quest log")
			.withTabOrder(90));
	}

	formations = std::make_shared<CToggleGroup>(0);
	auto tightFormation = std::make_shared<CToggleButton>(Point(481, 483), AnimationPath::builtin("hsbtns6.def"), std::make_pair(heroscrn[23], heroscrn[29]), 0, EShortcut::HERO_TIGHT_FORMATION);
	tightFormation->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("radio")
		.withName("Tight formation")
		.withDescription("Set army to tight formation")
		.withTabOrder(93));
	formations->addToggle(0, tightFormation);
	
	auto looseFormation = std::make_shared<CToggleButton>(Point(481, 519), AnimationPath::builtin("hsbtns7.def"), std::make_pair(heroscrn[24], heroscrn[30]), 0, EShortcut::HERO_LOOSE_FORMATION);
	looseFormation->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("radio")
		.withName("Loose formation")
		.withDescription("Set army to loose formation")
		.withTabOrder(94));
	formations->addToggle(1, looseFormation);

	if(hero->getCommander())
	{
		commanderButton = std::make_shared<CButton>(Point(317, 18), AnimationPath::builtin("heroCommander"), CButton::tooltipLocalized("vcmi.heroWindow.openCommander"), [&](){ commanderWindow(); }, EShortcut::HERO_COMMANDER);
		commanderButton->setOverlay(std::make_shared<CPicture>(ImagePath::builtin("heroWindow/commanderButtonIcon")));
		commanderButton->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("button")
			.withName("Commander")
			.withDescription("Open commander window")
			.withTabOrder(5));
	}

	//right list of heroes
	for(int i=0; i < std::min(GAME->interface()->cb->howManyHeroes(false), 8); i++)
		heroList.push_back(std::make_shared<CHeroSwitcher>(this, Point(612, 87 + i * 54), GAME->interface()->cb->getHeroBySerial(i, false)));

	//areas
	portraitArea = std::make_shared<LRClickableAreaWText>(Rect(18, 18, 58, 64));
	portraitArea->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("button")
		.withName("Hero portrait")
		.withDescription("Click to view hero biography")
		.withTabOrder(3));
	portraitArea->addUsedEvents(KEYBOARD);
	portraitImage = std::make_shared<CAnimImage>(AnimationPath::builtin("PortraitsLarge"), 0, 0, 19, 19);

	for(int v = 0; v < GameConstants::PRIMARY_SKILLS; ++v)
	{
		auto area = std::make_shared<LRClickableAreaWTextComp>(Rect(30 + 70 * v, 109, 42, 64), ComponentType::PRIM_SKILL);
		area->text = LIBRARY->generaltexth->arraytxt[2+v];
		area->component.subType = PrimarySkill(v);
		area->hoverText = boost::str(boost::format(LIBRARY->generaltexth->heroscrn[1]) % LIBRARY->generaltexth->primarySkillNames[v]);
		
		// Add accessibility info for primary skill areas
		area->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("status")
			.withName(LIBRARY->generaltexth->primarySkillNames[v])
			.withDescription(area->text)
			.withTabOrder(10 + v));
		
		// Make primary skill areas focusable
		area->addUsedEvents(KEYBOARD);
		
		primSkillAreas.push_back(area);

		auto value = std::make_shared<CLabel>(53 + 70 * v, 166, FONT_SMALL, ETextAlignment::CENTER);
		primSkillValues.push_back(value);
	}

	primSkillImages.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("PSKIL42"), 0, 0, 32, 111));
	primSkillImages.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("PSKIL42"), 1, 0, 102, 111));
	primSkillImages.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("PSKIL42"), 2, 0, 172, 111));
	primSkillImages.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("PSKIL42"), 3, 0, 162, 230));
	primSkillImages.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("PSKIL42"), 4, 0, 20, 230));
	primSkillImages.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("PSKIL42"), 5, 0, 242, 111));

	specImage = std::make_shared<CAnimImage>(AnimationPath::builtin("UN44"), 0, 0, 18, 180);
	specArea = std::make_shared<LRClickableAreaWText>(Rect(18, 180, 136, 42), LIBRARY->generaltexth->heroscrn[27]);
	specArea->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("status")
		.withName("Special ability")
		.withDescription(LIBRARY->generaltexth->heroscrn[27])
		.withTabOrder(15));
	specArea->addUsedEvents(KEYBOARD);
	specName = std::make_shared<CLabel>(69, 205);

	expArea = std::make_shared<LRClickableAreaWText>(Rect(18, 228, 136, 42), LIBRARY->generaltexth->heroscrn[9]);
	expArea->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("status")
		.withName("Experience")
		.withDescription(LIBRARY->generaltexth->heroscrn[9])
		.withTabOrder(16));
	expArea->addUsedEvents(KEYBOARD);
	
	morale = std::make_shared<MoraleLuckBox>(true, Rect(175, 179, 53, 45));
	morale->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("status")
		.withName("Morale")
		.withTabOrder(17));
	morale->addUsedEvents(KEYBOARD);
	
	luck = std::make_shared<MoraleLuckBox>(false, Rect(233, 179, 53, 45));
	luck->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("status")
		.withName("Luck")
		.withTabOrder(18));
	luck->addUsedEvents(KEYBOARD);
	
	spellPointsArea = std::make_shared<LRClickableAreaWText>(Rect(162,228, 136, 42), LIBRARY->generaltexth->heroscrn[22]);
	spellPointsArea->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("status")
		.withName("Spell points")
		.withDescription(LIBRARY->generaltexth->heroscrn[22])
		.withTabOrder(19));
	spellPointsArea->addUsedEvents(KEYBOARD);

	expValue = std::make_shared<CLabel>(68, 252);
	manaValue = std::make_shared<CLabel>(211, 252);

	if(hero->secSkills.size() > 8)
	{
		auto divisionRoundUp = [](int x, int y){ return (x + (y - 1)) / y; };
		int lines = divisionRoundUp(hero->secSkills.size(), 2);
		secSkillSlider = std::make_shared<CSlider>(Point(284, 276), 189, [this](int val){ CHeroWindow::update(); }, 4, lines, 0, Orientation::VERTICAL, CSlider::BROWN);
		secSkillSlider->setPanningStep(48);
		secSkillSlider->setScrollBounds(Rect(-266, 0, secSkillSlider->pos.x - pos.x + secSkillSlider->pos.w, secSkillSlider->pos.h));
		
		// Add accessibility to skills slider
		secSkillSlider->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("slider")
			.withName("Skills scroll")
			.withDescription("Scroll to view additional secondary skills")
			.withTabOrder(25));
	}

	for(int i = 0; i < std::min<size_t>(hero->secSkills.size(), 8u); ++i)
	{
		bool isSmallBox = (secSkillSlider && i%2 == 1);
		Rect r = Rect(i%2 == 0  ?  18  :  162,  276 + 48 * (i/2), isSmallBox ? 120 : 136,  42);
		secSkills.emplace_back(std::make_shared<CSecSkillPlace>(r.topLeft(), CSecSkillPlace::ImageSize::MEDIUM));

		int x = (i % 2) ? 212 : 68;
		int y = 280 + 48 * (i/2);
		int width = isSmallBox ? 71 : 87;

		secSkillValues.push_back(std::make_shared<CLabel>(x, y, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, "", width));
		secSkillNames.push_back(std::make_shared<CLabel>(x, y+20, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::WHITE, "", width));
	}

	// various texts
	auto attackLabel = std::make_shared<CLabel>(52, 99, FONT_SMALL, ETextAlignment::CENTER, Colors::YELLOW, LIBRARY->generaltexth->jktexts[1]);
	attackLabel->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("text")
		.withName("Attack label")
		.withDescription("Label for attack skill")
		.withTabOrder(6));
	labels.push_back(attackLabel);
	
	auto defenseLabel = std::make_shared<CLabel>(123, 99, FONT_SMALL, ETextAlignment::CENTER, Colors::YELLOW, LIBRARY->generaltexth->jktexts[2]);
	defenseLabel->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("text")
		.withName("Defense label")
		.withDescription("Label for defense skill")
		.withTabOrder(7));
	labels.push_back(defenseLabel);
	
	auto powerLabel = std::make_shared<CLabel>(193, 99, FONT_SMALL, ETextAlignment::CENTER, Colors::YELLOW, LIBRARY->generaltexth->jktexts[3]);
	powerLabel->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("text")
		.withName("Power label")
		.withDescription("Label for spell power")
		.withTabOrder(8));
	labels.push_back(powerLabel);
	
	auto knowledgeLabel = std::make_shared<CLabel>(262, 99, FONT_SMALL, ETextAlignment::CENTER, Colors::YELLOW, LIBRARY->generaltexth->jktexts[4]);
	knowledgeLabel->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("text")
		.withName("Knowledge label")
		.withDescription("Label for knowledge skill")
		.withTabOrder(9));
	labels.push_back(knowledgeLabel);

	auto specialtyLabel = std::make_shared<CLabel>(69, 183, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::YELLOW, LIBRARY->generaltexth->jktexts[5]);
	specialtyLabel->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("text")
		.withName("Specialty label")
		.withDescription("Label for hero specialty")
		.withTabOrder(14));
	labels.push_back(specialtyLabel);
	
	auto experienceLabel = std::make_shared<CLabel>(69, 232, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::YELLOW, LIBRARY->generaltexth->jktexts[6]);
	experienceLabel->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("text")
		.withName("Experience label")
		.withDescription("Label for experience points")
		.withTabOrder(15));
	labels.push_back(experienceLabel);
	
	auto manaLabel = std::make_shared<CLabel>(213, 232, FONT_SMALL, ETextAlignment::TOPLEFT, Colors::YELLOW, LIBRARY->generaltexth->jktexts[7]);
	manaLabel->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("text")
		.withName("Mana label")
		.withDescription("Label for mana points")
		.withTabOrder(18));
	labels.push_back(manaLabel);

	// Add Movement points display (placed in lower area near buttons)
	movementArea = std::make_shared<LRClickableAreaWText>(Rect(314, 395, 100, 30), "Movement Points");
	movementArea->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("status")
		.withName("Movement Points")
		.withDescription("Hero's current and maximum movement points")
		.withTabOrder(89));
	movementArea->addUsedEvents(KEYBOARD);
	movementValue = std::make_shared<CLabel>(365, 399, FONT_SMALL, ETextAlignment::CENTER, Colors::WHITE);
	labels.push_back(std::make_shared<CLabel>(365, 383, FONT_SMALL, ETextAlignment::CENTER, Colors::YELLOW, "Movement"));

	CHeroWindow::update();
}

void CHeroWindow::update()
{
	OBJECT_CONSTRUCTION;

	CWindowWithArtifacts::update();
	auto & heroscrn = LIBRARY->generaltexth->heroscrn;
	assert(curHero);

	name->setText(curHero->getNameTranslated());
	if(name->getAccessibilityInfo())
	{
		auto currentInfo = name->getAccessibilityInfo();
		name->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
			.withValue(curHero->getNameTranslated()));
	}
	
	title->setText((boost::format(LIBRARY->generaltexth->allTexts[342]) % curHero->level % curHero->getClassNameTranslated()).str());
	if(title->getAccessibilityInfo())
	{
		auto currentInfo = title->getAccessibilityInfo();
		title->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
			.withValue(title->getText()));
	}

	specArea->text = curHero->getHeroType()->getSpecialtyDescriptionTranslated();
	specImage->setFrame(curHero->getHeroType()->imageIndex);
	specName->setText(curHero->getHeroType()->getSpecialtyNameTranslated());
	
	// Update special ability accessibility
	{
		auto currentInfo = specArea->getAccessibilityInfo();
		if(currentInfo)
		{
			specArea->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
				.withValue(curHero->getHeroType()->getSpecialtyNameTranslated())
				.withDescription("Special ability: " + curHero->getHeroType()->getSpecialtyNameTranslated() + ". " + specArea->text));
		}
	}

	tacticsButton = std::make_shared<CToggleButton>(Point(539, 483), AnimationPath::builtin("hsbtns8.def"), std::make_pair(heroscrn[26], heroscrn[31]), 0, EShortcut::HERO_TOGGLE_TACTICS);
	tacticsButton->addHoverText(EButtonState::HIGHLIGHTED, LIBRARY->generaltexth->heroscrn[25]);
	tacticsButton->setAccessibilityInfo(UIAccessibilityInfo()
		.withRole("checkbox")
		.withName("Tactics")
		.withDescription("Toggle tactics mode for battles")
		.withTabOrder(95));
	tacticsButton->setSelected(curHero->tacticFormationEnabled);

	dismissButton->addHoverText(EButtonState::NORMAL, boost::str(boost::format(LIBRARY->generaltexth->heroscrn[16]) % curHero->getNameTranslated() % curHero->getClassNameTranslated()));
	portraitArea->hoverText = boost::str(boost::format(LIBRARY->generaltexth->allTexts[15]) % curHero->getNameTranslated() % curHero->getClassNameTranslated());
	portraitArea->text = curHero->getBiographyTranslated();
	portraitImage->setFrame(curHero->getIconIndex());
	
	// Update portrait area accessibility
	if(portraitArea->getAccessibilityInfo())
	{
		auto currentInfo = portraitArea->getAccessibilityInfo();
		portraitArea->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
			.withValue(curHero->getNameTranslated())
			.withDescription("Portrait of " + curHero->getNameTranslated() + ". Click to view biography"));
	}

	{
		if(!garr)
		{
			bool removableTroops = curHero->getOwner() == GAME->interface()->playerID;
			std::string helpBox = heroscrn[32];
			boost::algorithm::replace_first(helpBox, "%s", LIBRARY->generaltexth->allTexts[43]);

			garr = std::make_shared<CGarrisonInt>(Point(15, 485), 8, Point(), curHero, nullptr, removableTroops);
			auto split = std::make_shared<CButton>(Point(539, 519), AnimationPath::builtin("hsbtns9.def"), CButton::tooltip(LIBRARY->generaltexth->allTexts[256], helpBox), [this](){ garr->splitClick(); }, EShortcut::HERO_ARMY_SPLIT);
			garr->addSplitBtn(split);
			
			// Add accessibility info to garrison
			garr->setAccessibilityInfo(UIAccessibilityInfo()
				.withRole("group")
				.withName("Hero army")
				.withDescription("Hero's army units")
				.withTabOrder(70));
		}
		if(!arts)
		{
			arts = std::make_shared<CArtifactsOfHeroMain>(Point(-65, -8));
			arts->clickPressedCallback = [this](const CArtPlace & artPlace, const Point & cursorPosition){clickPressedOnArtPlace(curHero, artPlace.slot, true, false, false, cursorPosition);};
			arts->showPopupCallback = [this](CArtPlace & artPlace, const Point & cursorPosition){showArtifactAssembling(*arts, artPlace, cursorPosition);};
			arts->gestureCallback = [this](const CArtPlace & artPlace, const Point & cursorPosition){showQuickBackpackWindow(curHero, artPlace.slot, cursorPosition);};
			arts->setHero(curHero);
			addSet(arts);
			enableKeyboardShortcuts();
		}

		int serial = GAME->interface()->cb->getHeroSerial(curHero, false);

		listSelection.reset();
		if(serial >= 0)
			listSelection = std::make_shared<CPicture>(ImagePath::builtin("HPSYYY"), 612, 33 + serial * 54);
	}

	//primary skills support
	for(size_t g=0; g<primSkillAreas.size(); ++g)
	{
		int value = curHero->getPrimSkillLevel(static_cast<PrimarySkill>(g));
		primSkillAreas[g]->component.value = value;
		primSkillValues[g]->setText(std::to_string(value));
		
		// Update accessibility info with current value
		std::string skillName = LIBRARY->generaltexth->primarySkillNames[g];
		std::string description = skillName + ": " + std::to_string(value);
		auto currentInfo = primSkillAreas[g]->getAccessibilityInfo();
		if(currentInfo)
		{
			primSkillAreas[g]->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
				.withValue(std::to_string(value))
				.withDescription(description));
		}
	}

	//secondary skills support
	for(size_t g=0; g < secSkills.size(); ++g)
	{
		int offset = secSkillSlider ? secSkillSlider->getValue() * 2 : 0;
		if(curHero->secSkills.size() < g + offset + 1)
		{
			secSkillNames[g]->setText("");
			secSkillValues[g]->setText("");
			secSkills[g]->setSkill(SecondarySkill::NONE);
			
			// Clear accessibility info for empty slots
			secSkills[g]->setAccessibilityInfo(UIAccessibilityInfo()
				.withRole("skill_slot")
				.withName("Empty skill slot")
				.withDescription("No skill learned in this slot")
				.withState("empty")
				.withTabOrder(20 + g));
			break;
		}
		SecondarySkill skill = curHero->secSkills[g + offset].first;
		int	level = curHero->getSecSkillLevel(skill);
		std::string skillName = skill.toEntity(LIBRARY)->getNameTranslated();
		std::string skillValue = LIBRARY->generaltexth->levels[level-1];

		secSkillNames[g]->setText(skillName);
		secSkillValues[g]->setText(skillValue);
		secSkills[g]->setSkill(skill, level);
		
		// Add accessibility info for secondary skills
		std::string fullSkillName = skillName + " - " + skillValue;
		std::string skillDescription = skill.toSkill()->getDescriptionTranslated(level);
		
		secSkills[g]->setAccessibilityInfo(UIAccessibilityInfo()
			.withRole("skill_slot")
			.withName(fullSkillName)
			.withDescription(skillDescription)
			.withTabOrder(20 + g));
	}

	std::ostringstream expstr;
	expstr << curHero->exp;
	expValue->setText(expstr.str());

	std::ostringstream manastr;
	manastr << curHero->mana << '/' << curHero->manaLimit();
	manaValue->setText(manastr.str());

	//printing experience - original format does not support ui64
	expArea->text = LIBRARY->generaltexth->allTexts[2];
	boost::replace_first(expArea->text, "%d", std::to_string(curHero->level));
	boost::replace_first(expArea->text, "%d", std::to_string(LIBRARY->heroh->reqExp(curHero->level+1)));
	boost::replace_first(expArea->text, "%d", std::to_string(curHero->exp));
	
	// Update experience accessibility
	{
		auto currentInfo = expArea->getAccessibilityInfo();
		if(currentInfo)
		{
			expArea->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
				.withValue(std::to_string(curHero->exp))
				.withDescription("Experience: " + std::to_string(curHero->exp) + ". " + expArea->text));
		}
	}

	//printing spell points, boost::format can't be used due to locale issues
	spellPointsArea->text = LIBRARY->generaltexth->allTexts[205];
	boost::replace_first(spellPointsArea->text, "%s", curHero->getNameTranslated());
	boost::replace_first(spellPointsArea->text, "%d", std::to_string(curHero->mana));
	boost::replace_first(spellPointsArea->text, "%d", std::to_string(curHero->manaLimit()));
	
	// Update spell points accessibility
	{
		auto currentInfo = spellPointsArea->getAccessibilityInfo();
		if(currentInfo)
		{
			spellPointsArea->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
				.withValue(std::to_string(curHero->mana) + "/" + std::to_string(curHero->manaLimit()))
				.withDescription("Spell points: " + std::to_string(curHero->mana) + " of " + std::to_string(curHero->manaLimit()) + ". " + spellPointsArea->text));
		}
	}

	//if we have exchange window with this curHero open
	bool noDismiss=false;

	for(auto cew : ENGINE->windows().findWindows<CExchangeWindow>())
	{
		if (cew->holdsGarrison(curHero))
			noDismiss = true;
	}

	//if player only have one hero and no towns
	if(!GAME->interface()->cb->howManyTowns() && GAME->interface()->cb->howManyHeroes() == 1)
		noDismiss = true;

	if(curHero->isMissionCritical())
		noDismiss = true;

	dismissButton->block(noDismiss);

	if(curHero->valOfBonuses(BonusType::BEFORE_BATTLE_REPOSITION) == 0)
	{
		tacticsButton->block(true);
	}
	else
	{
		tacticsButton->block(false);
		tacticsButton->addCallback([&](bool on){curHero->tacticFormationEnabled = on;});
		
		// Update tactics button accessibility state
		if(tacticsButton->getAccessibilityInfo())
		{
			auto currentInfo = tacticsButton->getAccessibilityInfo();
			tacticsButton->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
				.withState(curHero->tacticFormationEnabled ? "checked" : ""));
		}
	}

	formations->resetCallback();
	//setting formations
	formations->setSelected(curHero->formation == EArmyFormation::TIGHT ? 1 : 0);
	formations->addCallback([this](int value){ GAME->interface()->cb->setFormation(curHero, static_cast<EArmyFormation>(value));});
	
	// Update formation buttons accessibility state
	auto tightFormationButton = std::dynamic_pointer_cast<CToggleButton>(formations->buttons[0]);
	auto looseFormationButton = std::dynamic_pointer_cast<CToggleButton>(formations->buttons[1]);
	
	if(tightFormationButton && tightFormationButton->getAccessibilityInfo())
	{
		auto currentInfo = tightFormationButton->getAccessibilityInfo();
		tightFormationButton->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
			.withState(curHero->formation == EArmyFormation::TIGHT ? "checked" : ""));
	}
	if(looseFormationButton && looseFormationButton->getAccessibilityInfo())
	{
		auto currentInfo = looseFormationButton->getAccessibilityInfo();
		looseFormationButton->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
			.withState(curHero->formation == EArmyFormation::LOOSE ? "checked" : ""));
	}

	morale->set(curHero);
	luck->set(curHero);
	
	// Update morale and luck accessibility with current values
	if(morale)
	{
		auto currentInfo = morale->getAccessibilityInfo();
		if(currentInfo)
		{
			int moraleValue = morale->component.value.value_or(0);
			morale->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
				.withValue(std::to_string(moraleValue))
				.withDescription("Morale: " + std::to_string(moraleValue) + ". " + morale->hoverText));
		}
	}
	if(luck)
	{
		auto currentInfo = luck->getAccessibilityInfo();
		if(currentInfo)
		{
			int luckValue = luck->component.value.value_or(0);
			luck->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
				.withValue(std::to_string(luckValue))
				.withDescription("Luck: " + std::to_string(luckValue) + ". " + luck->hoverText));
		}
	}

	// Update movement points
	if(movementValue)
	{
		std::ostringstream moveStr;
		moveStr << curHero->movementPointsRemaining() << "/" << curHero->movementPointsLimit(true);
		movementValue->setText(moveStr.str());
		
		// Update movement points accessibility info
		if(movementArea->getAccessibilityInfo())
		{
			auto currentInfo = movementArea->getAccessibilityInfo();
			movementArea->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
				.withValue(moveStr.str())
				.withDescription("Movement Points: " + moveStr.str() + " remaining"));
		}
	}

	redraw();
}

void CHeroWindow::dismissCurrent()
{
	GAME->interface()->showYesNoDialog(LIBRARY->generaltexth->allTexts[22], [this]()
		{
			arts->putBackPickedArtifact();
			close();
			GAME->interface()->cb->dismissHero(curHero);
			arts->setHero(nullptr);
		}, nullptr);
}

void CHeroWindow::createBackpackWindow()
{
	ENGINE->windows().createAndPushWindow<CHeroBackpackWindow>(curHero, artSets);
}

void CHeroWindow::commanderWindow()
{
	const auto pickedArtInst = getPickedArtifact();
	const auto hero = getHeroPickedArtifact();

	if(pickedArtInst)
	{
		const auto freeSlot = ArtifactUtils::getArtAnyPosition(curHero->getCommander(), pickedArtInst->getTypeId());
		if(vstd::contains(ArtifactUtils::commanderSlots(), freeSlot)) // We don't want to put it in commander's backpack!
		{
			ArtifactLocation dst(curHero->id, freeSlot);
			dst.creature = SlotID::COMMANDER_SLOT_PLACEHOLDER;
			GAME->interface()->cb->swapArtifacts(ArtifactLocation(hero->id, ArtifactPosition::TRANSITION_POS), dst);
		}
	}
	else
	{
		ENGINE->windows().createAndPushWindow<CStackWindow>(curHero->getCommander(), false);
	}
}

void CHeroWindow::updateGarrisons()
{
	garr->recreateSlots();
	
	// Update garrison accessibility info
	if(garr && garr->getAccessibilityInfo())
	{
		auto currentInfo = garr->getAccessibilityInfo();
		int totalUnits = 0;
		for(const auto & slot : curHero->Slots())
		{
			if(slot.second)
				totalUnits++;
		}
		garr->setAccessibilityInfo(UIAccessibilityInfo(*currentInfo)
			.withValue(std::to_string(totalUnits) + " army slots occupied")
			.withDescription("Hero's army units: " + std::to_string(totalUnits) + " slots occupied"));
	}
	
	morale->set(curHero);
}

bool CHeroWindow::holdsGarrison(const CArmedInstance * army)
{
	return army == curHero;
}
