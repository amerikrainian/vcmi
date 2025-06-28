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
#include "../../../SpeechCore/include/SpeechCore.h"

#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <codecvt>
#include <locale>

#ifdef VCMI_WINDOWS
#include <windows.h>
#endif

VCMI_LIB_NAMESPACE_BEGIN

// Implementation struct for PIMPL idiom
struct AccessibilityManager::Implementation
{
	mutable std::mutex speechMutex;
	std::atomic<bool> speechInitialized{false};
	std::atomic<bool> currentlySpeaking{false};
	
	// For UTF-8 to UTF-16 conversion
#ifdef VCMI_WINDOWS
	// Windows-specific conversion members
#else
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
#endif
	
	Implementation()
	{
#ifdef VCMI_WINDOWS
		// Initialize COM for SAPI on Windows
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
		{
			logGlobal->warn("AccessibilityManager: Failed to initialize COM, error: 0x%08X", hr);
		}
#endif
	}
	
	~Implementation()
	{
		if (speechInitialized)
		{
			Speech_Free();
		}
#ifdef VCMI_WINDOWS
		// Uninitialize COM on Windows
		CoUninitialize();
#endif
	}
	
	void ensureInitialized()
	{
		if (!speechInitialized)
		{
			Speech_Init();
			
#ifdef VCMI_WINDOWS
			// On Windows, prefer SAPI as it's more reliable than screen readers for game use
			Speech_Prefer_Sapi(true);
#endif
			
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
{
	logGlobal->info("AccessibilityManager: Initialized");
}

AccessibilityManager::~AccessibilityManager()
{
	shutdown();
}

void AccessibilityManager::init()
{
	std::lock_guard<std::mutex> lock(impl->speechMutex);
	
	try
	{
		impl->ensureInitialized();
		
		if (Speech_Is_Loaded())
		{
			logGlobal->info("AccessibilityManager: SpeechCore initialized successfully");
			
			// Convert wide string to UTF-8 for logging
			const wchar_t* driverNameWide = Speech_Current_Driver();
			if (driverNameWide)
			{
#ifdef VCMI_WINDOWS
				// Convert UTF-16 to UTF-8 for logging
				int size = WideCharToMultiByte(CP_UTF8, 0, driverNameWide, -1, nullptr, 0, nullptr, nullptr);
				if (size > 0)
				{
					std::string driverName(size - 1, '\0');
					WideCharToMultiByte(CP_UTF8, 0, driverNameWide, -1, &driverName[0], size, nullptr, nullptr);
					logGlobal->info("AccessibilityManager: Using screen reader: %s", driverName.c_str());
				}
				else
				{
					logGlobal->info("AccessibilityManager: Using screen reader: (name conversion failed)");
				}
#else
				// Use standard conversion on non-Windows platforms
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
#ifdef VCMI_WINDOWS
			// Try to initialize SAPI directly as a fallback
			logGlobal->info("AccessibilityManager: Attempting direct SAPI initialization");
			Sapi_Init();
			if (Speech_Sapi_Loaded())
			{
				logGlobal->info("AccessibilityManager: SAPI initialized successfully as fallback");
			}
			else
			{
				logGlobal->error("AccessibilityManager: SAPI initialization also failed");
			}
#endif
		}
	}
	catch (const std::exception& e)
	{
		logGlobal->error("AccessibilityManager: Exception during initialization: %s", e.what());
	}
}

void AccessibilityManager::shutdown()
{
	std::lock_guard<std::mutex> lock(impl->speechMutex);
	
	if (impl->speechInitialized)
	{
		stopSpeaking();
		
		// Clear any pending announcements
		while (!announcementQueue.empty())
		{
			announcementQueue.pop();
		}
		
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
		std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Give time for the announcement
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
	
	std::lock_guard<std::mutex> lock(impl->speechMutex);
	
	impl->ensureInitialized();
	
	if (!Speech_Is_Loaded())
	{
		logGlobal->warn("AccessibilityManager: Screen reader not available");
#ifdef VCMI_WINDOWS
		// Log available drivers for debugging
		int driverCount = Speech_Get_Drivers();
		logGlobal->info("AccessibilityManager: Available drivers: %d", driverCount);
		for (int i = 0; i < driverCount; i++)
		{
			const wchar_t* driverName = Speech_Get_Driver(i);
			if (driverName)
			{
				int size = WideCharToMultiByte(CP_UTF8, 0, driverName, -1, nullptr, 0, nullptr, nullptr);
				if (size > 0)
				{
					std::string name(size - 1, '\0');
					WideCharToMultiByte(CP_UTF8, 0, driverName, -1, &name[0], size, nullptr, nullptr);
					logGlobal->info("AccessibilityManager: Driver %d: %s", i, name.c_str());
				}
			}
		}
#endif
		return;
	}
	
	// Sanitize the text for speech
	std::string cleanText = impl->sanitizeForSpeech(text);
	if (cleanText.empty())
		return;
	
	if (interrupt)
	{
		// Clear the queue if we're interrupting
		while (!announcementQueue.empty())
		{
			announcementQueue.pop();
		}
		
		// Stop current speech
		Speech_Stop();
		impl->currentlySpeaking = false;
	}
	
	// Add to queue with default priority (0 for now)
	announcementQueue.push(std::make_pair(cleanText, 0));
	
	// Process immediately if not speaking
	if (!impl->currentlySpeaking)
	{
		processAnnouncements();
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
	
	// TODO: Implement hover text extraction from CIntObject
	// This would require access to tooltip text or other hover information
}

void AccessibilityManager::processAnnouncements()
{
	if (!screenReaderEnabled)
		return;
	
	std::lock_guard<std::mutex> lock(impl->speechMutex);
	
	while (!announcementQueue.empty() && !impl->currentlySpeaking)
	{
		auto announcement = announcementQueue.front();
		announcementQueue.pop();
		
		std::wstring wideText = impl->toWideString(announcement.first);
		if (!wideText.empty())
		{
			impl->currentlySpeaking = true;
			
			bool success = Speech_Output(wideText.c_str(), false);
			if (!success)
			{
				logGlobal->warn("AccessibilityManager: Failed to speak text");
#ifdef VCMI_WINDOWS
				// Get more detailed error information on Windows
				DWORD error = GetLastError();
				logGlobal->warn("AccessibilityManager: Windows error code: %d", error);
#endif
				impl->currentlySpeaking = false;
			}
			else
			{
				// Start a thread to monitor when speech completes
				std::thread([this]() {
					while (Speech_Is_Speaking())
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(50));
					}
					impl->currentlySpeaking = false;
					
					// Check if there are more announcements
					if (!announcementQueue.empty())
					{
						processAnnouncements();
					}
				}).detach();
			}
		}
	}
}

void AccessibilityManager::stopSpeaking()
{
	if (!impl->speechInitialized)
		return;
	
	std::lock_guard<std::mutex> lock(impl->speechMutex);
	
	Speech_Stop();
	impl->currentlySpeaking = false;
	
	// Clear the queue
	while (!announcementQueue.empty())
	{
		announcementQueue.pop();
	}
}

bool AccessibilityManager::isSpeaking() const
{
	if (!screenReaderEnabled || !impl->speechInitialized)
		return false;
	
	std::lock_guard<std::mutex> lock(impl->speechMutex);
	return Speech_Is_Speaking();
}

std::string AccessibilityManager::getAccessibleText(const CIntObject* element) const
{
	if (!element)
		return "";
	
	// TODO: This requires integration with CIntObject to get accessibility info
	// For now, return a placeholder
	std::string text;
	
	// Try to get role
	std::string role = getElementRole(element);
	if (!role.empty())
	{
		text = role;
	}
	
	// TODO: Add element name, state, value from UIAccessibilityInfo when it's integrated
	
	return text;
}

std::string AccessibilityManager::getElementRole(const CIntObject* element) const
{
	if (!element)
		return "";
	
	// TODO: Determine role based on element type
	// This would require runtime type information or virtual methods in CIntObject
	
	return "UI element"; // Placeholder
}

AccessibilityManager& AccessibilityManager::getInstance()
{
	static AccessibilityManager instance;
	return instance;
}

VCMI_LIB_NAMESPACE_END