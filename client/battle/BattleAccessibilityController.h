/*
 * BattleAccessibilityController.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../../lib/battle/BattleHex.h"
#include "../../lib/battle/PossiblePlayerBattleAction.h"
#include "../gui/Shortcut.h"

VCMI_LIB_NAMESPACE_BEGIN
class CStack;
class CSpell;
struct CObstacleInstance;
VCMI_LIB_NAMESPACE_END

class BattleInterface;

class BattleAccessibilityController
{
public:
    enum class Mode
    {
        NORMAL_NAVIGATION,  // Default hex navigation
        SPELL_TARGETING     // Targeting mode for spells only
    };
    
private:
    BattleInterface & owner;
    BattleHex currentHex;
    BattleHex savedUnitHex; // To return to after escape
    bool hexNavigationMode;
    bool spellTargetingMode;
    Mode currentMode;

    const CSpell* targetingSpell;
    
    // Obstacle cycling
    std::vector<std::shared_ptr<const CObstacleInstance>> cachedObstacles;
    int currentObstacleIndex;
    
    std::string getAnnouncedCoordinates(BattleHex hex) const;
    std::string formatUnitAnnouncement(const CStack* stack) const;
    std::string getTerrainName(BattleHex hex) const;
    BattleHex::EDir mapKeyToHexDirection(EShortcut key) const;

    void executeClickAction();
    
public:
    BattleAccessibilityController(BattleInterface & owner);
    
    void handleHexNavigation(EShortcut key);
    void handleUnitMovement(EShortcut key);
    void handleEnterKey();
    void handleEscapeKey();
    void handleObstacleCycling(EShortcut key);
    
    void announceHexContent(BattleHex hex);
    std::string getObstacleName(const CObstacleInstance* obstacle) const;
    void announceUnitInfo(const CStack* stack, int detailLevel);
    void announceCombatEvent(const std::string& event);
    void enterSpellTargetingMode(const CSpell* spell);
    void exitSpellTargetingMode();
    void activateHexNavigation();
    void deactivateHexNavigation();
    void silentlyPositionCursorOnUnit(const CStack* stack);
    
    bool isHexNavigationActive() const { return hexNavigationMode; }
    bool isSpellTargetingActive() const { return spellTargetingMode; }
    BattleHex getCurrentHex() const { return currentHex; }
    void setCurrentHex(BattleHex hex);
    Mode getCurrentMode() const { return currentMode; }
    
    void announceInitialBattleState();
    void announceTurnStart(const CStack* activeStack);
    void announceActionResult(const std::string& action, bool success);
    void announceCurrentTurn();
    void announceTurnQueue();
};