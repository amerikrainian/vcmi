/*
 * BattleAccessibilityController.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "BattleAccessibilityController.h"
#include "BattleInterface.h"
#include "BattleFieldController.h"
#include "BattleStacksController.h"
#include "BattleActionsController.h"
#include "BattleWindow.h"
#include "../CPlayerInterface.h"
#include "../gui/AccessibilityManager.h"
#include "../gui/WindowHandler.h"
#include "../GameEngine.h"
#include "../../lib/battle/CUnitState.h"
#include "../../lib/battle/CPlayerBattleCallback.h"
#include "../../lib/battle/CObstacleInstance.h"
#include "../../lib/battle/BattleAction.h"
#include "../../lib/spells/CSpellHandler.h"
#include "../../lib/bonuses/BonusCustomTypes.h"
#include "../../lib/bonuses/BonusSelector.h"
#include "../../lib/CStack.h"
#include "../../lib/GameSettings.h"
#include "../../lib/constants/Enumerations.h"

BattleAccessibilityController::BattleAccessibilityController(BattleInterface & owner)
    : owner(owner)
    , currentHex(BattleHex::INVALID)
    , savedUnitHex(BattleHex::INVALID)
    , hexNavigationMode(false)
    , spellTargetingMode(false)
    , currentMode(Mode::NORMAL_NAVIGATION)
    , targetingSpell(nullptr)
{
}

std::string BattleAccessibilityController::getAnnouncedCoordinates(BattleHex hex) const
{
    if (!hex.isValid())
        return "";
    
    // Convert to 1-based coordinates as specified in combat_plan.md
    return std::to_string(hex.getX() + 1) + ", " + std::to_string(hex.getY() + 1);
}

std::string BattleAccessibilityController::formatUnitAnnouncement(const CStack* stack) const
{
    if (!stack)
        return "";
    
    std::string result = std::to_string(stack->getCount()) + " ";
    
    // Add "enemy" prefix if not on current player's side
    auto activeUnit = owner.getBattle()->battleActiveUnit();
    if (activeUnit && stack->unitSide() != activeUnit->unitSide())
        result += "enemy ";
    
    result += stack->unitType()->getNamePluralTranslated();
    
    // Add damage information if any units are wounded
    auto firstAlive = stack->getFirstHPleft();
    if (firstAlive < stack->getMaxHealth())
    {
        // Count units at each health level
        std::map<int32_t, int> healthCounts;
        for (int i = 0; i < stack->getCount(); i++)
        {
            auto health = (i == 0) ? firstAlive : stack->getMaxHealth();
            healthCounts[health]++;
        }
        
        // Format as "3×48, 5×30" style
        bool first = true;
        for (const auto& [health, count] : healthCounts)
        {
            if (health == stack->getMaxHealth())
                continue; // Don't show full health units
                
            result += first ? ", " : ", ";
            result += std::to_string(count) + "×" + std::to_string(health);
            first = false;
        }
    }
    
    // Add status effects
    std::vector<SpellID> activeSpells = stack->activeSpells();
    if (!activeSpells.empty())
    {
        for (size_t i = 0; i < activeSpells.size(); i++)
        {
            const CSpell* spell = activeSpells[i].toSpell();
            if (!spell)
                continue;
                
            result += ", " + spell->getNameTranslated();
            
            // Get duration
            auto spellBonuses = stack->getBonuses(Selector::source(BonusSource::SPELL_EFFECT, BonusSourceID(activeSpells[i])));
            if (!spellBonuses->empty() && spellBonuses->front()->turnsRemain > 0)
            {
                result += " (" + std::to_string(spellBonuses->front()->turnsRemain) + " turns)";
            }
        }
    }
    
    return result;
}

std::string BattleAccessibilityController::getTerrainName(BattleHex hex) const
{
    // This will be expanded once we have access to battlefield terrain info
    // For now, return empty string
    return "";
}

BattleHex::EDir BattleAccessibilityController::mapKeyToHexDirection(EShortcut key) const
{
    switch(key)
    {
        case EShortcut::BATTLE_HEX_UP_LEFT:     // Q
        case EShortcut::BATTLE_MOVE_UNIT_UP_LEFT:  // Shift+Q
            return BattleHex::TOP_LEFT;
        case EShortcut::BATTLE_HEX_UP_RIGHT:    // E
        case EShortcut::BATTLE_MOVE_UNIT_UP_RIGHT:  // Shift+E
            return BattleHex::TOP_RIGHT;
        case EShortcut::BATTLE_HEX_LEFT:        // A
        case EShortcut::BATTLE_MOVE_UNIT_LEFT:  // Shift+A
            return BattleHex::LEFT;
        case EShortcut::BATTLE_HEX_RIGHT:       // D
        case EShortcut::BATTLE_MOVE_UNIT_RIGHT:  // Shift+D
            return BattleHex::RIGHT;
        case EShortcut::BATTLE_HEX_DOWN_LEFT:   // Z
        case EShortcut::BATTLE_MOVE_UNIT_DOWN_LEFT:  // Shift+Z
            return BattleHex::BOTTOM_LEFT;
        case EShortcut::BATTLE_HEX_DOWN_RIGHT:  // C
        case EShortcut::BATTLE_MOVE_UNIT_DOWN_RIGHT:  // Shift+C
            return BattleHex::BOTTOM_RIGHT;
        default:
            return BattleHex::NONE;
    }
}

void BattleAccessibilityController::handleHexNavigation(EShortcut key)
{
    if (!AccessibilityManager::getInstance().isKeyboardNavigationEnabled())
        return;
    
    // Allow hex navigation in normal mode and spell targeting mode
    if (!hexNavigationMode && currentMode != Mode::SPELL_TARGETING)
        return;
    
    auto direction = mapKeyToHexDirection(key);
    if (direction == BattleHex::NONE)
        return;
    
    BattleHex newHex = currentHex.cloneInDirection(direction, false);
    
    if (newHex.isValid() && newHex != currentHex)
    {
        currentHex = newHex;
        announceHexContent(currentHex);
        owner.actionsController->onHexHovered(currentHex);
    }
    // Silent failure if invalid move (edge collision)
}

void BattleAccessibilityController::handleUnitMovement(EShortcut key)
{
    if (!AccessibilityManager::getInstance().isKeyboardNavigationEnabled())
        return;
    
    auto activeUnit = owner.getBattle()->battleActiveUnit();
    if (!activeUnit)
    {
        AccessibilityManager::getInstance().announce("No active unit");
        return;
    }
    
    // Map Shift+QWEASD to movement directions
    auto direction = mapKeyToHexDirection(key);
    if (direction == BattleHex::NONE)
        return;
    
    const CStack* activeStack = dynamic_cast<const CStack*>(activeUnit);
    if (!activeStack)
        return;
        
    BattleHex targetHex = activeStack->getPosition().cloneInDirection(direction, activeStack->doubleWide());
    
    if (!targetHex.isValid())
        return; // Silent failure for edge collision
    
    // Check if movement is possible
    auto reachability = owner.getBattle()->getReachability(activeStack);
    if (!reachability.isReachable(targetHex))
    {
        // Check what's blocking
        const CStack* blockingStack = owner.getBattle()->battleGetStackByPos(targetHex);
        if (blockingStack)
        {
            AccessibilityManager::getInstance().announce("Blocked by " + blockingStack->unitType()->getNamePluralTranslated());
        }
        else
        {
            AccessibilityManager::getInstance().announce("Cannot reach");
        }
        return;
    }
    
    // Execute movement
    owner.stackActivated(activeStack);
    owner.actionsController->onHexLeftClicked(targetHex);
}

void BattleAccessibilityController::announceHexContent(BattleHex hex)
{
    if (!hex.isValid())
        return;
    
    std::string announcement = getAnnouncedCoordinates(hex);
    
    // Add terrain if available
    std::string terrain = getTerrainName(hex);
    if (!terrain.empty())
        announcement += ", " + terrain;
    
    // Check for units
    const CStack* stack = owner.getBattle()->battleGetStackByPos(hex);
    if (stack)
    {
        announcement += ", " + formatUnitAnnouncement(stack);
    }
    
    // Check for obstacles
    auto obstacles = owner.getBattle()->battleGetAllObstacles();
    for (const auto& obstacle : obstacles)
    {
        if (obstacle->getBlockedTiles().contains(hex))
        {
            announcement += ", obstacle";
            break;
        }
    }
    
    // Add the same action feedback that mouse users get visually
    const auto* activeUnit = owner.stacksController->getActiveStack();
    if (activeUnit && !owner.tacticsMode)
    {
        auto action = owner.actionsController->selectAction(hex);
        
        if (owner.actionsController->actionIsLegal(action, hex))
        {
            // Get the status message that mouse users see in status bar
            std::string statusMsg = owner.actionsController->actionGetStatusMessage(action, hex);
            if (!statusMsg.empty())
            {
                announcement += ". " + statusMsg;
            }
        }
        else
        {
            // Get the blocked message that mouse users see
            std::string blockedMsg = owner.actionsController->actionGetStatusMessageBlocked(action, hex);
            if (!blockedMsg.empty())
            {
                announcement += ". " + blockedMsg;
            }
        }
    }
    
    AccessibilityManager::getInstance().announce(announcement, true);
}

void BattleAccessibilityController::announceUnitInfo(const CStack* stack, int detailLevel)
{
    if (!stack)
        return;
    
    std::string announcement;
    
    switch(detailLevel)
    {
        case 1: // F1 - Attack and Defense
            announcement = stack->unitType()->getNameSingularTranslated() + 
                          ": Attack " + std::to_string(stack->getAttack(false)) +
                          ", Defense " + std::to_string(stack->getDefense(false));
            break;
            
        case 2: // F2 - Damage and Speed
            announcement = stack->unitType()->getNameSingularTranslated() +
                          ": Damage " + std::to_string(stack->getMinDamage(false)) +
                          "-" + std::to_string(stack->getMaxDamage(false)) +
                          ", Speed " + std::to_string(stack->getMovementRange());
            break;
            
        case 3: // F3 - Health and Shots
            announcement = stack->unitType()->getNameSingularTranslated() +
                          ": Health " + std::to_string(stack->getFirstHPleft()) +
                          "/" + std::to_string(stack->getMaxHealth());
            if (stack->isShooter())
                announcement += ", Shots " + std::to_string(stack->shots.available());
            break;
            
        case 4: // F4 - Status effects
            announcement = stack->unitType()->getNameSingularTranslated() + ": ";
            // TODO: List all active effects with durations
            announcement += "No active effects";
            break;
            
        case 5: // F5 - Special abilities
            announcement = stack->unitType()->getNameSingularTranslated() + ": ";
            // TODO: List special abilities
            announcement += "Special abilities";
            break;
            
        case 6: // F6 - Morale and Luck
            announcement = stack->unitType()->getNameSingularTranslated() +
                          ": Morale " + std::to_string(stack->moraleVal()) +
                          ", Luck " + std::to_string(stack->luckVal());
            break;
    }
    
    AccessibilityManager::getInstance().announce(announcement);
}

void BattleAccessibilityController::announceCombatEvent(const std::string& event)
{
    AccessibilityManager::getInstance().announce(event, true);
}

void BattleAccessibilityController::enterSpellTargetingMode(const CSpell* spell)
{
    spellTargetingMode = true;
    targetingSpell = spell;
    activateHexNavigation();
    
    std::string announcement = "Targeting mode for " + spell->getNameTranslated();
    announcement += ". " + spell->getDescriptionTranslated(0); // Basic level description
    AccessibilityManager::getInstance().announce(announcement);
}

void BattleAccessibilityController::exitSpellTargetingMode()
{
    spellTargetingMode = false;
    targetingSpell = nullptr;
}

void BattleAccessibilityController::activateHexNavigation()
{
    if (!currentHex.isValid())
    {
        // Start at center of battlefield
        currentHex = BattleHex(GameConstants::BFIELD_WIDTH / 2, GameConstants::BFIELD_HEIGHT / 2);
    }
    hexNavigationMode = true;
    announceHexContent(currentHex);
    owner.actionsController->onHexHovered(currentHex);
}

void BattleAccessibilityController::deactivateHexNavigation()
{
    hexNavigationMode = false;
    owner.actionsController->onHexHovered(BattleHex::INVALID);
}

void BattleAccessibilityController::setCurrentHex(BattleHex hex)
{
    currentHex = hex;
    if (hexNavigationMode)
        owner.actionsController->onHexHovered(currentHex);
}

void BattleAccessibilityController::announceInitialBattleState()
{
    if (!AccessibilityManager::getInstance().isScreenReaderEnabled())
        return;
    
    std::string announcement = "Battle started. ";
    
    // Announce armies
    auto attackerStacks = owner.getBattle()->battleGetAllStacks();
    int attackerCount = 0, defenderCount = 0;
    
    for (const auto& stack : attackerStacks)
    {
        if (stack->unitSide() == BattleSide::ATTACKER)
            attackerCount++;
        else
            defenderCount++;
    }
    
    announcement += "Attacker: " + std::to_string(attackerCount) + " stacks. ";
    announcement += "Defender: " + std::to_string(defenderCount) + " stacks.";
    
    AccessibilityManager::getInstance().announce(announcement);
}

void BattleAccessibilityController::announceTurnStart(const CStack* activeStack)
{
    if (!activeStack)
        return;
    
    std::string announcement;
    auto currentActive = owner.getBattle()->battleActiveUnit();
    if (currentActive && activeStack->unitSide() == currentActive->unitSide())
        announcement = "Your ";
    else
        announcement = "Enemy ";
    
    announcement += activeStack->unitType()->getNamePluralTranslated() + "'s turn";
    
    AccessibilityManager::getInstance().announce(announcement, true);
}

void BattleAccessibilityController::announceActionResult(const std::string& action, bool success)
{
    if (success)
        AccessibilityManager::getInstance().announce(action + " successful");
    else
        AccessibilityManager::getInstance().announce(action + " failed");
}

void BattleAccessibilityController::handleEnterKey()
{
    if (!AccessibilityManager::getInstance().isKeyboardNavigationEnabled())
        return;
    
    switch (currentMode)
    {
        case Mode::NORMAL_NAVIGATION:
            // Enter = Left Click
            executeClickAction();
            break;
            
        case Mode::SPELL_TARGETING:
            // Let existing spell system handle this
            break;
    }
}

void BattleAccessibilityController::handleEscapeKey()
{
    switch (currentMode)
    {
        case Mode::NORMAL_NAVIGATION:
            // Let normal escape handling occur (combat menu)
            break;
            
        case Mode::SPELL_TARGETING:
            // Let existing spell system handle escape
            exitSpellTargetingMode();
            break;
    }
}

void BattleAccessibilityController::executeClickAction()
{
    if (!currentHex.isValid())
        return;
    
    // Simple: Enter key = Left click on the hex
    // Let the game's existing action system handle everything
    owner.actionsController->onHexLeftClicked(currentHex);
}

// All complex action selection logic removed - now we just use Enter = Left Click!