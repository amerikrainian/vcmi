/*
 * FocusManager.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "FocusManager.h"

#include "CIntObject.h"
#include "AccessibilityManager.h"
#include "WindowHandler.h"
#include "EventDispatcher.h"
#include "GameEngine.h"
#include "../CPlayerInterface.h"
#include "../adventureMap/AdventureMapInterface.h"
#include "../../lib/logging/CLogger.h"

#include <algorithm>

FocusManager::FocusManager()
    : focusedElement(nullptr)
{
}

FocusManager::~FocusManager()
{
}

void FocusManager::buildFocusableList(CIntObject* root)
{
    if (!root)
        return;
    
    // Check if this element is focusable
    if (canReceiveFocus(root))
    {
        focusableElements.push_back(root);
    }
    
    // Recursively process children
    for (auto child : root->children)
    {
        buildFocusableList(child);
    }
}

void FocusManager::sortFocusableElements()
{
    std::sort(focusableElements.begin(), focusableElements.end(), 
        [](const CIntObject* a, const CIntObject* b) 
        {
            auto aInfo = a->getAccessibilityInfo();
            auto bInfo = b->getAccessibilityInfo();
            
            // Elements without accessibility info go last
            if (!aInfo && !bInfo) return false;
            if (!aInfo) return false;
            if (!bInfo) return true;
            
            // Elements with -1 tabOrder go last
            if (aInfo->tabOrder < 0 && bInfo->tabOrder < 0) return false;
            if (aInfo->tabOrder < 0) return false;
            if (bInfo->tabOrder < 0) return true;
            
            // Sort by tabOrder
            return aInfo->tabOrder < bInfo->tabOrder;
        });
}

bool FocusManager::canReceiveFocus(const CIntObject* element) const
{
    if (!element)
        return false;
    
    // Use the element's own isFocusable method
    return element->isFocusable();
}

CIntObject* FocusManager::getNextFocusable(CIntObject* current, bool reverse)
{
    if (focusableElements.empty())
        return nullptr;
    
    if (!current)
    {
        // No current focus, return first/last element
        return reverse ? focusableElements.back() : focusableElements.front();
    }
    
    // Find current element in the list
    auto it = std::find(focusableElements.begin(), focusableElements.end(), current);
    if (it == focusableElements.end())
    {
        // Current element not in list, return first/last
        return reverse ? focusableElements.back() : focusableElements.front();
    }
    
    if (reverse)
    {
        // Move to previous element
        if (it == focusableElements.begin())
            return focusableElements.back(); // Wrap to end
        else
            return *(--it);
    }
    else
    {
        // Move to next element
        ++it;
        if (it == focusableElements.end())
            return focusableElements.front(); // Wrap to beginning
        else
            return *it;
    }
}

void FocusManager::setFocus(CIntObject* element)
{
    if (focusedElement == element)
        return;
    
    // Clear focus from previous element
    if (focusedElement)
    {
        focusedElement->onFocusLost();
    }
    
    focusedElement = element;
    
    // Set focus to new element
    if (focusedElement)
    {
        focusedElement->onFocusGained();
        
        // Set focus in accessibility manager
        if (AccessibilityManager::getInstance().isKeyboardNavigationEnabled())
        {
            AccessibilityManager::getInstance().setFocus(focusedElement);
        }
    }
}

void FocusManager::moveFocusNext()
{
    updateFocusableElements();
    
    if (focusableElements.empty())
        return;
    
    CIntObject* next = getNextFocusable(focusedElement, false);
    if (next)
        setFocus(next);
}

void FocusManager::moveFocusPrevious()
{
    updateFocusableElements();
    
    if (focusableElements.empty())
        return;
    
    CIntObject* prev = getNextFocusable(focusedElement, true);
    if (prev)
        setFocus(prev);
}

void FocusManager::clearFocus()
{
    setFocus(nullptr);
}

void FocusManager::updateFocusableElements()
{
    focusableElements.clear();
    
    
    // Get the topmost window or the main interface
    auto windows = ENGINE->windows().getWindowsArray();
    
    CIntObject* rootObject = nullptr;
    
    bool hasModalWindow = false;
    
    if (!windows.empty())
    {
        // Check if the topmost window is modal (blocks interaction with adventure map)
        // Build list from topmost window
        // IShowActivatable is the base interface, CIntObject inherits from it
        if (auto* intObject = dynamic_cast<CIntObject*>(windows.back()))
        {
            // Check if it's a blocking window (usually dialogs are)
            // For now, assume all windows are blocking
            hasModalWindow = true;
            rootObject = intObject;
        }
    }
    
    if (!hasModalWindow && adventureInt)
    {
        rootObject = adventureInt.get();
    }
    
    if (rootObject)
    {
        buildFocusableList(rootObject);
    }
    else
    {
        logGlobal->warn("FocusManager: No root object found for focus traversal");
    }
    
    // Sort by tab order
    sortFocusableElements();

    if (focusedElement && !canReceiveFocus(focusedElement))
    {
        clearFocus();
    }
}

bool FocusManager::handleActivation()
{
    if (!focusedElement)
        return false;
    
    // Simulate a click on the focused element
    Point clickPos = focusedElement->pos.center();
    ENGINE->events().dispatchMouseLeftButtonPressed(clickPos, 0);
    ENGINE->events().dispatchMouseLeftButtonReleased(clickPos, 0);
    
    return true;
}

FocusManager& FocusManager::getInstance()
{
    static FocusManager instance;
    return instance;
}