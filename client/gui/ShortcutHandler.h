/*
 * ShortcutHandler.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#pragma once

#include <memory>

enum class EShortcut;

VCMI_LIB_NAMESPACE_BEGIN
class JsonNode;
VCMI_LIB_NAMESPACE_END

class ShortcutHandler
{
	std::multimap<std::string, EShortcut> mappedKeyboardShortcuts;
	std::multimap<std::string, EShortcut> mappedJoystickShortcuts;
	std::multimap<std::string, EShortcut> mappedJoystickAxes;
	
	// Store the full config for profile switching
	std::unique_ptr<JsonNode> configData;
	std::string currentProfile;

	std::multimap<std::string, EShortcut> loadShortcuts(const JsonNode & data) const;
	std::vector<EShortcut> translateShortcut(const std::multimap<std::string, EShortcut> & options, const std::string & key) const;

public:
	ShortcutHandler();

	/// returns list of shortcuts assigned to provided SDL keycode
	std::vector<EShortcut> translateKeycode(const std::string & key) const;

	std::vector<EShortcut> translateJoystickButton(const std::string & key) const;

	std::vector<EShortcut> translateJoystickAxis(const std::string & key) const;

	/// attempts to find shortcut by its unique identifier. Returns EShortcut::NONE on failure
	EShortcut findShortcut(const std::string & identifier ) const;
	
	/// Load shortcuts from a specific profile (e.g., "keyboard" or "keyboard_combat_accessibility")
	void loadProfile(const std::string & profileName);
	
	/// Enable/disable combat accessibility shortcuts
	void setCombatAccessibilityMode(bool enable);
};
