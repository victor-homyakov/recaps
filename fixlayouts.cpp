#include "stdafx.h"
#include "fixlayouts.h"
#include "utils.h"

#define HKL_HEBREW  (HKL)0x040D040D
#define HKL_ENGLISH (HKL)0x04090409

///////////////////////////////////////////////////////////////////////////////
// Converts the text in the active window from one keyboard layout to another
// using the clipboard.
void ConvertSelectedTextInActiveWindow(HKL hklSource, HKL hklTarget)
{
	WCHAR* sourceText = NULL;
	WCHAR* targetText = NULL;
	const WCHAR dummy [] = L"__RECAPS__";

	// store previous clipboard data and set clipboard to dummy string
	ClipboardData prevClipboardData;
	StoreClipboardData(&prevClipboardData);

	SetClipboardText(dummy);

	// copy the selected text by simulating Ctrl-C
	SendKeyCombo(VK_CONTROL, 'C', FALSE);

	// wait until copy operation completes and get the copied data from the clipboard
	// this loop has the nice side effect of setting copyOK to FALSE if there's no
	// selected text, since nothing is copied and the clipboard still contains the
	// contents of `dummy`.
	BOOL copyOK = FALSE;
	for(int i = 0; i < 10; i++)
	{
		sourceText = GetClipboardText();
		if (sourceText && wcscmp(sourceText, dummy) != 0)
		{
			copyOK = TRUE;
			break;
		}
		else
		{
			free(sourceText);
			Sleep(30);
		}
	}

	if (copyOK)
	{
		// if the string only matches one particular layout, use it
		// otherwise use the provided layout
		int matches = 0;
		HKL hklDetected = DetectLayoutFromString(sourceText, &matches);
		if (matches == 1)
			hklSource = hklDetected;

		// convert the text between layouts
		size_t length = wcslen(sourceText);
		targetText = (WCHAR*)malloc(sizeof(WCHAR) * (length+1));
		size_t converted = LayoutConvertString(sourceText, targetText, length+1, hklSource, hklTarget);

		if (converted)
		{
			// put the converted string on the clipboard
			if (SetClipboardText(targetText))
			{
				// simulate Ctrl-V to paste the text, replacing the previous text
				SendKeyCombo(VK_CONTROL, 'V', FALSE);

				// let the application complete pasting before putting the old data back on the clipboard
				Sleep(REMOTE_APP_WAIT);
			}
		}

		// restore the original clipboard data
		RestoreClipboardData(&prevClipboardData);
		FreeClipboardData(&prevClipboardData);

		// free allocated memory
		free(sourceText);
		free(targetText);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Converts a character from one keyboard layout to another
WCHAR LayoutConvertChar(WCHAR ch, HKL hklSource, HKL hklTarget)
{
	// special handling for some ambivalent characters in hebrew layout
	if (hklSource == HKL_HEBREW && hklTarget == HKL_ENGLISH)
	{
		switch (ch)
		{
			case L'.':  return L'/';
			case L'/':  return L'q';
			case L'\'': return L'w';
			case L',':  return L'\'';
		}
	}
	else if (hklSource == HKL_ENGLISH && hklTarget == HKL_HEBREW)
	{
		switch (ch)
		{
			case L'/':  return L'.';
			case L'q':  return L'/';
			case L'w':  return L'\'';
			case L'\'': return L',';
		}
	}

	// get the virtual key code and the shift state using the character and the source keyboard layout
	SHORT vkAndShift = VkKeyScanEx(ch, hklSource);
	if (vkAndShift == -1)
		return 0; // no such char in source keyboard layout

	BYTE vk = LOBYTE(vkAndShift);
	BYTE shift = HIBYTE(vkAndShift);

	// convert the shift state returned from VkKeyScanEx to an array that represents the
	// key state usable with ToUnicodeEx that we'll be calling next
	BYTE keyState[256] = {0};
	if (shift & 1) keyState[VK_SHIFT] = 0x80;	// turn on high bit
	if (shift & 2) keyState[VK_CONTROL] = 0x80;
	if (shift & 4) keyState[VK_MENU] = 0x80;

	// convert virtual key and key state to a new character using the target keyboard layout
	WCHAR buffer[10] = {0};
	int result = ToUnicodeEx(vk, 0, keyState, buffer, 10, 0, hklTarget);

	// result can be more than 1 if the character in the source layout is represented by 
	// several characters in the target layout, but we ignore this to simplify the function.
	if (result == 1)
		return buffer[0];
	
	// conversion failed for some reason
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Converts a string from one keyboard layout to another
size_t LayoutConvertString(const WCHAR* str, WCHAR* buffer, size_t size, HKL hklSource, HKL hklTarget)
{
	size_t i;
	for (i = 0; i < wcslen(str) && i < size-1; i++)
	{
		WCHAR ch = LayoutConvertChar(str[i], hklSource, hklTarget);
		if (ch == 0)
			return 0;
		buffer[i] = ch;
	}
	buffer[i] = '\0';
	return i;
}

///////////////////////////////////////////////////////////////////////////////
// Goes through all the installed keyboard layouts and returns a layout that
// can generate the string. If not matching layout is found, returns NULL.
// If `multiple` isn't NULL it will be set to the number of matched layouts.
HKL DetectLayoutFromString(const WCHAR* str, int* pmatches)
{
	HKL result = NULL;
	HKL* hkls;
	UINT layoutCount;
	layoutCount = GetKeyboardLayoutList(0, NULL);
	hkls = (HKL*)malloc(sizeof(HKL) * layoutCount);
	GetKeyboardLayoutList(layoutCount, hkls);

	int matches = 0;
	for (size_t layout = 0; layout < layoutCount; layout++)
	{
		BOOL validLayout = TRUE;
		for (size_t i = 0; i < wcslen(str); i++)
		{
			UINT vk = VkKeyScanEx(str[i], hkls[layout]);
			if (vk == -1)
			{
				validLayout = FALSE;
				break;
			}
		}
		if (validLayout)
		{
			matches++;
			if (!result)
				result = hkls[layout];
		}
	}

	if (pmatches)
		*pmatches = matches;

	return result;
}

///////////////////////////////////////////////////////////////////////////////
// Stores the clipboard data in all its formats in `formats`.
// You must call FreeAllClipboardData on `formats` when it's no longer needed.
BOOL StoreClipboardData(ClipboardData* formats)
{
	if (OpenClipboard(NULL))
	{
		formats->count = CountClipboardFormats();
		formats->dataArray = (ClipboardFormat*)malloc(sizeof(ClipboardData) * formats->count);
		ZeroMemory(formats->dataArray, sizeof(ClipboardData) * formats->count); 
		int i = 0;
		UINT format = EnumClipboardFormats(0);
		while (format)
		{
			HANDLE dataHandle = GetClipboardData(format);
			LPVOID source = GlobalLock(dataHandle);
			size_t size = GlobalSize(dataHandle);
			formats->dataArray[i].format = format;
			formats->dataArray[i].dataHandle = GlobalAlloc(GHND, size);
			LPVOID dest = GlobalLock(formats->dataArray[i].dataHandle);
			CopyMemory(dest, source, size);
			GlobalUnlock(formats->dataArray[i].dataHandle);
			GlobalUnlock(dataHandle);

			// next format
			format = EnumClipboardFormats(format);
			i++;
		}
		CloseClipboard();
		return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// Restores the data in the clipboard from `formats` that was generated by
// StoreClipboardData.
BOOL RestoreClipboardData(const ClipboardData* formats)
{
	if (OpenClipboard(NULL))
	{
		EmptyClipboard();
		for (int i = 0; i < formats->count; i++)
		{
			SetClipboardData(formats->dataArray[i].format, formats->dataArray[i].dataHandle);
		}
		CloseClipboard();
		return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// Frees `formats` allocated by StoreClipboardData
void FreeClipboardData(ClipboardData* formats)
{
	free(formats->dataArray);
}

///////////////////////////////////////////////////////////////////////////////
// Gets unicode text from the clipboard. 
// You must free the returned string when you don't need it anymore.
WCHAR* GetClipboardText()
{
	WCHAR* text = NULL;
	if (OpenClipboard(NULL))
	{
		HANDLE handle = GetClipboardData(CF_UNICODETEXT);
		WCHAR* clipboardText = (WCHAR*)GlobalLock(handle);
		if (!clipboardText) return NULL;
		size_t size = sizeof(WCHAR) * (wcslen(clipboardText)+1);
		text = (WCHAR*)malloc(size);
		if (!text) return NULL;
		memcpy(text, clipboardText, size);
		CloseClipboard();
	}
	return text;
}

///////////////////////////////////////////////////////////////////////////////
// Puts unicode text on the clipboard
BOOL SetClipboardText(const WCHAR* text)
{
	if (OpenClipboard(NULL))
	{
		size_t size = sizeof(WCHAR) * (wcslen(text)+1);
		HANDLE handle = GlobalAlloc(GHND, size);
		WCHAR* clipboardText = (WCHAR*)GlobalLock(handle);
		memcpy(clipboardText, text, size);
		GlobalUnlock(handle);
		EmptyClipboard();
		SetClipboardData(CF_UNICODETEXT, handle);
		CloseClipboard();
		return TRUE;
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////
// Simulates a key press in the active window
void SendKey(BYTE vk, BOOL extended)
{
	keybd_event(vk, 0, extended ? KEYEVENTF_EXTENDEDKEY : 0, 0);
	keybd_event(vk, 0, KEYEVENTF_KEYUP | (extended ? KEYEVENTF_EXTENDEDKEY : 0), 0);
}

///////////////////////////////////////////////////////////////////////////////
// Simulates a key combination (such as Ctrl+X) in the active window
void SendKeyCombo(BYTE vkModifier, BYTE vk, BOOL extended)
{
	BOOL modPressed = (GetKeyState(vkModifier) & 0x80000000) > 0;
	if (!modPressed)
		keybd_event(vkModifier, 0, 0, 0);
	keybd_event(vk, 0, extended ? KEYEVENTF_EXTENDEDKEY : 0, 0);
	if (!modPressed)
		keybd_event(vkModifier, 0, KEYEVENTF_KEYUP, 0);
	keybd_event(vk, 0, KEYEVENTF_KEYUP | (extended ? KEYEVENTF_EXTENDEDKEY : 0), 0);
}

