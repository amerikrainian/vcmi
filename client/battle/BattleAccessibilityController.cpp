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
#include "../../lib/CConfigHandler.h"

#include <algorithm>

BattleAccessibilityController::BattleAccessibilityController(BattleInterface & owner)
    : owner(owner)
    , currentHex(BattleHex::INVALID)
    , savedUnitHex(BattleHex::INVALID)
    , hexNavigationMode(false)
    , spellTargetingMode(false)
    , currentMode(Mode::NORMAL_NAVIGATION)
    , targetingSpell(nullptr)
    , currentObstacleIndex(-1)
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

std::string BattleAccessibilityController::getObstacleName(const CObstacleInstance* obstacle) const
{
    if (!obstacle)
        return "obstacle";
    
    // Handle special obstacle types first
    if (obstacle->obstacleType == CObstacleInstance::MOAT)
        return "moat";
    
    // For spell-created obstacles
    if (obstacle->obstacleType == CObstacleInstance::SPELL_CREATED)
    {
        auto spellObstacle = dynamic_cast<const SpellCreatedObstacle*>(obstacle);
        if (spellObstacle && spellObstacle->trigger != SpellID::NONE)
        {
            // Get the spell name from the spell handler
            try
            {
                const CSpell* spell = spellObstacle->trigger.toSpell();
                if (spell)
                    return spell->getNameTranslated();
            }
            catch (const std::exception&)
            {
                // Fall back to generic name if spell lookup fails
            }
            return "magical obstacle";
        }
    }
    
    // For regular obstacles, parse the animation name
    std::string animName = obstacle->getAnimation().getName();
    
    // Remove file extension if present
    size_t dotPos = animName.find('.');
    if (dotPos != std::string::npos)
        animName = animName.substr(0, dotPos);
    
    // Convert animation prefixes to descriptive names
    // Special case for uppercase variations
    if (animName.find("OBGrS") == 0) return "grass";
    if (animName.find("OBLvL") == 0) return "lava rock";
    
    // Terrain-specific obstacles (Dirt terrain)
    if (animName.find("ObDino") == 0) return "dinosaur bones";
    if (animName.find("ObSkel") == 0) return "skeleton";
    if (animName.find("ObBDT") == 0) return "battlefield debris";
    if (animName.find("ObDRck") == 0 || animName.find("ObDRk") == 0) return "dark rock";
    if (animName.find("ObDSh") == 0) return "dead shrub";
    if (animName.find("ObDTF") == 0) return "dead tree";
    if (animName.find("ObDtL") == 0) return "large dirt mound";
    if (animName.find("ObDtS") == 0) return "dirt mound";
    
    // Desert/Sand obstacles
    if (animName.find("ObDsM") == 0) return "desert mountain";
    if (animName.find("ObDsS") == 0) return "sand dune";
    
    // Grass obstacles
    if (animName.find("ObGLg") == 0) return "log";
    if (animName.find("ObGRk") == 0) return "grass rock";
    if (animName.find("ObGSt") == 0) return "stone";
    if (animName.find("ObGrL") == 0) return "large grass patch";
    if (animName.find("ObGrS") == 0 || animName.find("ObGrss") == 0 || animName.find("ObGras") == 0) return "grass";
    
    // Snow obstacles
    if (animName.find("ObSnL") == 0) return "large snowdrift";
    if (animName.find("ObSnS") == 0) return "snowdrift";
    
    // Swamp obstacles
    if (animName.find("ObSwL") == 0) return "large swamp";
    if (animName.find("ObSwS") == 0) return "swamp";
    
    // Rough terrain obstacles
    if (animName.find("ObRgL") == 0) return "large rocks";
    if (animName.find("ObRgS") == 0) return "rocks";
    
    // Subterranean obstacles
    if (animName.find("ObSuS") == 0) return "stalagmite";
    
    // Lava obstacles
    if (animName.find("ObLvL") == 0) return "large lava rock";
    if (animName.find("ObLvS") == 0) return "lava rock";
    
    // Special battlefield obstacles
    if (animName.find("ObBhL") == 0 || animName.find("ObBhS") == 0) return "beach obstacle";
    if (animName.find("ObBtS") == 0) return "ship debris";
    
    // Special terrain obstacles
    if (animName.find("ObCFL") == 0 || animName.find("ObCFs") == 0) return "clover";
    if (animName.find("ObLPL") == 0 || animName.find("ObLPs") == 0) return "lucid pool";
    if (animName.find("ObFFL") == 0 || animName.find("ObFFs") == 0) return "fire";
    if (animName.find("ObRLL") == 0 || animName.find("ObRLs") == 0) return "rocky outcrop";
    if (animName.find("ObMCL") == 0 || animName.find("ObMCs") == 0) return "magic cloud";
    if (animName.find("ObHGs") == 0) return "holy artifact";
    if (animName.find("ObEFs") == 0) return "evil fog";
    
    // Additional patterns from original code (keeping for compatibility)
    if (animName.find("ObBone") == 0) return "bones";
    if (animName.find("ObSkul") == 0) return "skull";
    if (animName.find("ObLRck") == 0 || animName.find("ObLRk") == 0) return "large rock";
    if (animName.find("ObRock") == 0 || animName.find("ObRck") == 0) return "rock";
    if (animName.find("ObStne") == 0 || animName.find("ObStn") == 0) return "stone";
    if (animName.find("ObCrys") == 0) return "crystal";
    if (animName.find("ObTree") == 0 || animName.find("ObTre") == 0) return "tree";
    if (animName.find("ObDead") == 0 || animName.find("ObDeaT") == 0) return "dead tree";
    if (animName.find("ObStump") == 0 || animName.find("ObStmp") == 0) return "tree stump";
    if (animName.find("ObBrsh") == 0 || animName.find("ObBush") == 0) return "brush";
    if (animName.find("ObCact") == 0) return "cactus";
    if (animName.find("ObSnag") == 0) return "snag";
    if (animName.find("ObLog") == 0) return "log";
    if (animName.find("ObMoss") == 0) return "moss";
    if (animName.find("ObMush") == 0) return "mushroom";
    if (animName.find("ObPlnt") == 0) return "plant";
    if (animName.find("ObShip") == 0 || animName.find("ObWrck") == 0) return "shipwreck";
    if (animName.find("ObKetl") == 0 || animName.find("ObKettle") == 0) return "kettle";
    if (animName.find("ObTent") == 0) return "tent";
    if (animName.find("ObCart") == 0) return "cart";
    if (animName.find("ObBrl") == 0 || animName.find("ObBarrel") == 0) return "barrel";
    if (animName.find("ObSign") == 0) return "sign";
    if (animName.find("ObWall") == 0) return "wall";
    if (animName.find("ObRuin") == 0) return "ruins";
    if (animName.find("ObTomb") == 0) return "tomb";
    if (animName.find("ObGrave") == 0) return "grave";
    if (animName.find("ObLava") == 0) return "lava";
    if (animName.find("ObCld") == 0 || animName.find("ObCloud") == 0) return "cloud";
    if (animName.find("ObIce") == 0) return "ice";
    if (animName.find("ObSnow") == 0) return "snow";
    if (animName.find("ObSand") == 0) return "sand";
    if (animName.find("ObSwmp") == 0 || animName.find("ObSwamp") == 0) return "swamp";
    if (animName.find("ObCrat") == 0 || animName.find("ObCrater") == 0) return "crater";
    if (animName.find("ObFire") == 0) return "fire";
    if (animName.find("ObSmoke") == 0) return "smoke";
    
    // Default to generic "obstacle" if no match
    return "obstacle";
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
            announcement += ", " + getObstacleName(obstacle.get());
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
            
        case 4: // F4 - Reserved (was Status effects - now use right-click for full info)
            announcement = stack->unitType()->getNameSingularTranslated() + 
                          ": Press Enter or right-click for detailed information";
            break;
            
        case 5: // F5 - Reserved (was Special abilities - now use right-click for full info)
            announcement = stack->unitType()->getNameSingularTranslated() + 
                          ": Press Enter or right-click for detailed information";
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

void BattleAccessibilityController::silentlyPositionCursorOnUnit(const CStack* stack)
{
    if (!stack || !stack->getPosition().isValid())
        return;
    
    // Silently position the cursor on the unit's position
    currentHex = stack->getPosition();
    
    // Update hover state if hex navigation is active
    if (hexNavigationMode)
        owner.actionsController->onHexHovered(currentHex);
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

void BattleAccessibilityController::handleObstacleCycling(EShortcut key)
{
    auto allObstacles = owner.getBattle()->battleGetAllObstacles();

    cachedObstacles.clear();
    for (const auto& obstacle : allObstacles)
    {
        if (obstacle->obstacleType != CObstacleInstance::MOAT)
            cachedObstacles.push_back(obstacle);
    }
    
    if (cachedObstacles.empty())
    {
        AccessibilityManager::getInstance().announce("No obstacles on battlefield", true);
        return;
    }
    
    std::sort(cachedObstacles.begin(), cachedObstacles.end(), 
        [](const auto& a, const auto& b) { return a->pos.toInt() < b->pos.toInt(); });
    
    if (key == EShortcut::BATTLE_CYCLE_OBSTACLES_FORWARD)
    {
        currentObstacleIndex++;
        if (currentObstacleIndex >= static_cast<int>(cachedObstacles.size()))
            currentObstacleIndex = 0;
    }
    else
    {
        currentObstacleIndex--;
        if (currentObstacleIndex < 0)
            currentObstacleIndex = static_cast<int>(cachedObstacles.size()) - 1;
    }
    
    const auto& obstacle = cachedObstacles[currentObstacleIndex];
    currentHex = obstacle->pos;
    
    if (!hexNavigationMode)
        activateHexNavigation();
    
    owner.actionsController->onHexHovered(currentHex);
    
    std::string obstacleName = getObstacleName(obstacle.get());
    std::string announcement = obstacleName + " at " + getAnnouncedCoordinates(currentHex);
    announcement += " (" + std::to_string(currentObstacleIndex + 1) + " of " + 
                    std::to_string(cachedObstacles.size()) + ")";

    AccessibilityManager::getInstance().announce(announcement, true);
}

void BattleAccessibilityController::executeClickAction()
{
    if (!currentHex.isValid())
        return;
    
    // Simple: Enter key = Left click on the hex
    // Let the game's existing action system handle everything
    owner.actionsController->onHexLeftClicked(currentHex);
}

void BattleAccessibilityController::announceCurrentTurn()
{
    const auto* activeUnit = owner.getBattle()->battleActiveUnit();
    if (!activeUnit)
    {
        AccessibilityManager::getInstance().announce("No active unit");
        return;
    }
    
    const CStack* activeStack = dynamic_cast<const CStack*>(activeUnit);
    if (!activeStack)
        return;
    
    std::string announcement = "Current turn: " + formatUnitAnnouncement(activeStack);
    AccessibilityManager::getInstance().announce(announcement);
}

void BattleAccessibilityController::announceTurnQueue()
{
    int maxUnitsToAnnounce = 10; // Default to big queue size
    
    std::string queueSizeMode = settings["battle"]["queueSize"].String();
    bool embedQueue = false;
    
    if (queueSizeMode == "auto")
        embedQueue = ENGINE->screenDimensions().y < 700;
    else if (queueSizeMode == "small")
        embedQueue = true;
    
    if (embedQueue)
    {
        maxUnitsToAnnounce = std::clamp(static_cast<int>(settings["battle"]["queueSmallSlots"].Float()), 1, 19);
    }
    
    std::vector<battle::Units> queue;
    owner.getBattle()->battleGetTurnOrder(queue, maxUnitsToAnnounce, 10); // Get units over multiple turns
    if (queue.empty() || queue[0].empty())
    {
        AccessibilityManager::getInstance().announce("No units in turn queue");
        return;
    }
    
    std::string announcement = "Turn queue: ";
    
    int position = 1;
    for (const auto& turnUnits : queue)
    {
        for (const auto* unit : turnUnits)
        {
            if (position > maxUnitsToAnnounce)
                break;
                
            const CStack* stack = dynamic_cast<const CStack*>(unit);
            if (stack)
            {
                if (position > 1)
                    announcement += ", ";
                
                announcement += std::to_string(position) + ". ";
                
                announcement += std::to_string(stack->getCount()) + " ";
                auto activeUnit = owner.getBattle()->battleActiveUnit();
                if (activeUnit && stack->unitSide() != activeUnit->unitSide())
                    announcement += "enemy ";
                announcement += stack->unitType()->getNamePluralTranslated();
                
                position++;
            }
        }
        if (position > maxUnitsToAnnounce)
            break;
    }
    
    AccessibilityManager::getInstance().announce(announcement);
}

