/*
 * AccessibilityManager.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "AccessibilityManager.h"

#include "CIntObject.h"
#include "../../lib/texts/TextOperations.h"
#include "SpeechCore.h"

#include <string>
#include <codecvt>
#include <locale>
#include <SDL_timer.h>

#ifdef VCMI_WINDOWS
#include <windows.h>
#endif

VCMI_LIB_NAMESPACE_BEGIN

// Implementation struct for PIMPL idiom
struct AccessibilityManager::Implementation
{
	bool speechInitialized = false;
	
	// For UTF-8 to UTF-16 conversion
#ifndef VCMI_WINDOWS
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
#endif
	
	void ensureInitialized()
	{
		if (!speechInitialized)
		{
			Speech_Init();
			Speech_Detect_Driver();
			speechInitialized = true;
		}
	}
	
	std::wstring toWideString(const std::string& utf8String)
	{
		try
		{
#ifdef VCMI_WINDOWS
			// Use Windows MultiByteToWideChar for UTF-8 to UTF-16 conversion
			if (utf8String.empty())
				return L"";
			
			// First, get the required buffer size
			int size = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, nullptr, 0);
			if (size == 0)
			{
				logGlobal->error("Failed to get buffer size for UTF-8 to UTF-16 conversion");
				return L"";
			}
			
			// Allocate buffer and perform conversion
			std::wstring result(size - 1, L'\0'); // size includes null terminator
			int convertedSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, &result[0], size);
			if (convertedSize == 0)
			{
				logGlobal->error("Failed to convert UTF-8 to UTF-16");
				return L"";
			}
			
			return result;
#else
			return converter.from_bytes(utf8String);
#endif
		}
		catch (const std::exception& e)
		{
			logGlobal->error("Failed to convert string to wide string: %s", e.what());
			return L"";
		}
	}
	
	std::string sanitizeForSpeech(const std::string& text)
	{
		std::string result = text;
		
		// Remove color codes like {Red}, {White}, etc.
		size_t start = 0;
		while ((start = result.find('{', start)) != std::string::npos)
		{
			size_t end = result.find('}', start);
			if (end != std::string::npos)
			{
				result.erase(start, end - start + 1);
			}
			else
			{
				start++;
			}
		}
		
		// Remove multiple spaces
		size_t pos = 0;
		while ((pos = result.find("  ", pos)) != std::string::npos)
		{
			result.replace(pos, 2, " ");
		}
		
		// Trim leading and trailing spaces
		size_t first = result.find_first_not_of(' ');
		if (first == std::string::npos)
			return "";
		
		size_t last = result.find_last_not_of(' ');
		return result.substr(first, (last - first + 1));
	}
};

AccessibilityManager::AccessibilityManager()
	: impl(std::make_unique<Implementation>())
	, screenReaderEnabled(false)
	, keyboardNavigationEnabled(false)
	, announceHoverText(true)
	, focusedElement(nullptr)
	, lastAnnouncementTime(0)
{
	logGlobal->info("AccessibilityManager: Initialized");
}

AccessibilityManager::~AccessibilityManager()
{
	shutdown();
}

void AccessibilityManager::init()
{
	try
	{
		impl->ensureInitialized();
		
		if (Speech_Is_Loaded())
		{
			logGlobal->info("AccessibilityManager: SpeechCore initialized successfully");
			
			// Log which screen reader/TTS is being used
			const wchar_t* driverNameWide = Speech_Current_Driver();
			if (driverNameWide)
			{
#ifdef VCMI_WINDOWS
				int size = WideCharToMultiByte(CP_UTF8, 0, driverNameWide, -1, nullptr, 0, nullptr, nullptr);
				if (size > 0)
				{
					std::string driverName(size - 1, '\0');
					WideCharToMultiByte(CP_UTF8, 0, driverNameWide, -1, &driverName[0], size, nullptr, nullptr);
					logGlobal->info("AccessibilityManager: Using screen reader: %s", driverName.c_str());
				}
#else
				std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
				try
				{
					std::string driverName = conv.to_bytes(driverNameWide);
					logGlobal->info("AccessibilityManager: Using screen reader: %s", driverName.c_str());
				}
				catch (...)
				{
					logGlobal->info("AccessibilityManager: Using screen reader: (name conversion failed)");
				}
#endif
			}
		}
		else
		{
			logGlobal->warn("AccessibilityManager: SpeechCore failed to initialize");
		}
	}
	catch (const std::exception& e)
	{
		logGlobal->error("AccessibilityManager: Exception during initialization: %s", e.what());
	}
}

void AccessibilityManager::shutdown()
{
	if (impl->speechInitialized)
	{
		stopSpeaking();
		Speech_Free();
		impl->speechInitialized = false;
		logGlobal->info("AccessibilityManager: Shutdown complete");
	}
}

void AccessibilityManager::setScreenReaderEnabled(bool enabled)
{
	screenReaderEnabled = enabled;
	
	if (enabled)
	{
		init();
		announce("Screen reader enabled", true);
	}
	else
	{
		announce("Screen reader disabled", true);
		stopSpeaking();
	}
}

void AccessibilityManager::setKeyboardNavigationEnabled(bool enabled)
{
	keyboardNavigationEnabled = enabled;
	
	if (screenReaderEnabled)
	{
		announce(enabled ? "Keyboard navigation enabled" : "Keyboard navigation disabled", false);
	}
}

void AccessibilityManager::announce(const std::string& text, bool interrupt)
{
	if (!screenReaderEnabled || text.empty())
		return;
	
	impl->ensureInitialized();
	
	if (!Speech_Is_Loaded())
	{
		logGlobal->debug("AccessibilityManager: Screen reader not available");
		return;
	}
	
	std::string cleanText = impl->sanitizeForSpeech(text);
	if (cleanText.empty())
		return;
	
	// Prevent duplicate announcements within 100ms
	uint32_t currentTime = SDL_GetTicks();
	if (!interrupt && cleanText == lastAnnouncedText && currentTime - lastAnnouncementTime < 100)
	{
		logGlobal->trace("AccessibilityManager: Skipping duplicate announcement: %s", cleanText.c_str());
		return;
	}
	
	lastAnnouncedText = cleanText;
	lastAnnouncementTime = currentTime;
	
	std::wstring wideText = impl->toWideString(cleanText);
	if (!wideText.empty())
	{
		// Fire-and-forget approach - just send to screen reader
		bool success = Speech_Output(wideText.c_str(), interrupt);
		if (!success)
		{
			logGlobal->debug("AccessibilityManager: Failed to speak text: %s", cleanText.c_str());
		}
	}
}

void AccessibilityManager::announceElement(const CIntObject* element)
{
	if (!screenReaderEnabled || !element)
		return;
	
	std::string text = getAccessibleText(element);
	if (!text.empty())
	{
		announce(text, false);
	}
}

void AccessibilityManager::setFocus(CIntObject* element)
{
	if (focusedElement != element)
	{
		focusedElement = element;
		
		if (screenReaderEnabled && element)
		{
			announceElement(element);
		}
	}
}

void AccessibilityManager::handleHover(const CIntObject* element)
{
	if (!screenReaderEnabled || !announceHoverText || !element)
		return;
	
	// Get accessibility info from the element
	const UIAccessibilityInfo* info = element->getAccessibilityInfo();
	if (info && !info->name.empty())
	{
		announce(info->name, false);
	}
}

void AccessibilityManager::processAnnouncements()
{
	// No longer needed - fire-and-forget approach
}

void AccessibilityManager::stopSpeaking()
{
	if (!impl->speechInitialized)
		return;
	
	Speech_Stop();
}

bool AccessibilityManager::isSpeaking() const
{
	// Many screen readers don't support this, so just return false
	// This prevents code from waiting for speech to finish
	return false;
}

std::string AccessibilityManager::getAccessibleText(const CIntObject* element) const
{
	if (!element)
		return "";
	
	const UIAccessibilityInfo* info = element->getAccessibilityInfo();
	if (!info || !info->isAccessible)
		return "";
	
	std::string text;
	
	// Build accessible text from available information
	// Start with the role if no name is provided
	if (info->name.empty() && !info->role.empty())
	{
		text = info->role;
	}
	else if (!info->name.empty())
	{
		text = info->name;
		// Add role after name
		if (!info->role.empty())
		{
			text += ", ";
			text += info->role;
		}
	}
	
	if (!info->state.empty())
	{
		if (!text.empty())
			text += ", ";
		text += info->state;
	}
	
	if (!info->value.empty())
	{
		if (!text.empty())
			text += ", ";
		text += info->value;
	}
	
	if (!info->description.empty() && text != info->description)
	{
		if (!text.empty())
			text += ". ";
		text += info->description;
	}
	
	return text;
}

std::string AccessibilityManager::getElementRole(const CIntObject* element) const
{
	if (!element)
		return "";
	
	const UIAccessibilityInfo* info = element->getAccessibilityInfo();
	if (info)
		return info->role;
	
	return "";
}

AccessibilityManager& AccessibilityManager::getInstance()
{
	static AccessibilityManager instance;
	return instance;
}

VCMI_LIB_NAMESPACE_END