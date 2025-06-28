/*
 * FocusManager.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include <vector>
#include <memory>

VCMI_LIB_NAMESPACE_BEGIN

class CIntObject;
class AccessibilityManager;

/// Manages keyboard focus for UI elements
class FocusManager : boost::noncopyable
{
private:
    /// Currently focused element
    CIntObject* focusedElement;
    
    /// List of all focusable elements in the current context
    std::vector<CIntObject*> focusableElements;
    
    /// Build list of focusable elements from the root window
    void buildFocusableList(CIntObject* root);
    
    /// Sort focusable elements by their tab order
    void sortFocusableElements();
    
    /// Find next/previous focusable element
    CIntObject* getNextFocusable(CIntObject* current, bool reverse = false);
    
    /// Check if element can receive focus
    bool canReceiveFocus(const CIntObject* element) const;
    
public:
    FocusManager();
    ~FocusManager();
    
    /// Set focus to specific element
    void setFocus(CIntObject* element);
    
    /// Get currently focused element
    CIntObject* getFocusedElement() const { return focusedElement; }
    
    /// Move focus to next/previous element
    void moveFocusNext();
    void moveFocusPrevious();
    
    /// Clear focus from current element
    void clearFocus();
    
    /// Update focusable elements list (call when UI changes)
    void updateFocusableElements();
    
    /// Handle keyboard activation of focused element
    bool handleActivation();
    
    /// Get singleton instance
    static FocusManager& getInstance();
};

VCMI_LIB_NAMESPACE_END