/*
 * AccessibilityManager.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include <memory>
#include <string>
#include <queue>

VCMI_LIB_NAMESPACE_BEGIN

class CIntObject;

/// Manages screen reader and accessibility features for VCMI
class AccessibilityManager : boost::noncopyable
{
private:
    struct Implementation;
    std::unique_ptr<Implementation> impl;
    
    bool screenReaderEnabled;
    bool keyboardNavigationEnabled;
    bool announceHoverText;
    
    /// Currently focused UI element for keyboard navigation
    CIntObject* focusedElement;
    
    /// Track last announcement to prevent duplicates
    mutable std::string lastAnnouncedText;
    mutable uint32_t lastAnnouncementTime;
    
    /// Convert CIntObject to accessible text
    std::string getAccessibleText(const CIntObject* element) const;
    
    /// Get the role description for a UI element
    std::string getElementRole(const CIntObject* element) const;
    
public:
    AccessibilityManager();
    ~AccessibilityManager();
    
    /// Initialize the accessibility system
    void init();
    
    /// Shutdown the accessibility system
    void shutdown();
    
    /// Enable/disable screen reader support
    void setScreenReaderEnabled(bool enabled);
    bool isScreenReaderEnabled() const { return screenReaderEnabled; }
    
    /// Enable/disable keyboard navigation
    void setKeyboardNavigationEnabled(bool enabled);
    bool isKeyboardNavigationEnabled() const { return keyboardNavigationEnabled; }
    
    /// Announce text to screen reader
    void announce(const std::string& text, bool interrupt = false);
    
    /// Announce a UI element (reads its accessible text)
    void announceElement(const CIntObject* element);
    
    /// Handle focus change
    void setFocus(CIntObject* element);
    CIntObject* getFocusedElement() const { return focusedElement; }
    
    /// Handle hover events
    void handleHover(const CIntObject* element);
    
    /// Process queued announcements
    void processAnnouncements();
    
    /// Stop all current speech
    void stopSpeaking();
    
    /// Check if screen reader is currently speaking
    bool isSpeaking() const;
    
    /// Get singleton instance
    static AccessibilityManager& getInstance();
};

/// Accessibility information that can be attached to CIntObject
struct UIAccessibilityInfo
{
    std::string role;        /// UI element type (button, label, window, etc.)
    std::string name;        /// Accessible name/label
    std::string description; /// Extended description
    std::string value;       /// Current value (for sliders, progress bars, etc.)
    std::string state;       /// Current state (pressed, selected, disabled, etc.)
    bool isAccessible = true;/// Whether this element should be announced
    int tabOrder = -1;       /// Order in tab navigation (-1 = not in tab order)
    
    UIAccessibilityInfo() = default;
    
    UIAccessibilityInfo& withRole(const std::string& r) { role = r; return *this; }
    UIAccessibilityInfo& withName(const std::string& n) { name = n; return *this; }
    UIAccessibilityInfo& withDescription(const std::string& d) { description = d; return *this; }
    UIAccessibilityInfo& withValue(const std::string& v) { value = v; return *this; }
    UIAccessibilityInfo& withState(const std::string& s) { state = s; return *this; }
    UIAccessibilityInfo& withTabOrder(int order) { tabOrder = order; return *this; }
};

VCMI_LIB_NAMESPACE_END