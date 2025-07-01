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
#include "../gui/Shortcut.h"

VCMI_LIB_NAMESPACE_BEGIN
class CStack;
class CSpell;
VCMI_LIB_NAMESPACE_END

class BattleInterface;

class BattleAccessibilityController
{
private:
    BattleInterface & owner;
    BattleHex currentHex;
    bool hexNavigationMode;
    bool spellTargetingMode;
    const CSpell* targetingSpell;
    
    std::string getAnnouncedCoordinates(BattleHex hex) const;
    std::string formatUnitAnnouncement(const CStack* stack) const;
    std::string getTerrainName(BattleHex hex) const;
    BattleHex::EDir mapKeyToHexDirection(EShortcut key) const;
    
public:
    BattleAccessibilityController(BattleInterface & owner);
    
    void handleHexNavigation(EShortcut key);
    void handleUnitMovement(EShortcut key);
    void announceHexContent(BattleHex hex);
    void announceUnitInfo(const CStack* stack, int detailLevel);
    void announceCombatEvent(const std::string& event);
    void enterSpellTargetingMode(const CSpell* spell);
    void exitSpellTargetingMode();
    void activateHexNavigation();
    void deactivateHexNavigation();
    
    bool isHexNavigationActive() const { return hexNavigationMode; }
    bool isSpellTargetingActive() const { return spellTargetingMode; }
    BattleHex getCurrentHex() const { return currentHex; }
    void setCurrentHex(BattleHex hex);
    
    void announceInitialBattleState();
    void announceTurnStart(const CStack* activeStack);
    void announceActionResult(const std::string& action, bool success);
};