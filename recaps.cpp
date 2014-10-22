/******************************************************************************

Recaps - change language and keyboard layout using the CapsLock key.
Copyright (C) 2007 Eli Golovinsky

-------------------------------------------------------------------------------

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*******************************************************************************/

#include "stdafx.h"
#include "resource.h"
#include "trayicon.h"
#include "fixlayouts.h"

#define HELP_MESSAGE \
    L"Recaps allows you to quickly switch the current\r\n"\
	L"language using the Capslock key.\r\n"\
	L"\r\n"\
	L"Capslock changes the current keyboard laguange.\r\n"\
	L"Ctrl-Capslock fixes text you typed in the wrong laguange.\r\n"\
	L"Alt-Capslock is the old Capslock that lets you type in CAPITAL.\r\n"\
    L"\r\n"\
    L"http://www.gooli.org/blog/recaps\r\n\r\n"\
    L"Eli Golovinsky, Israel 2008\r\n"

#define HELP_TITLE \
    L"Recaps 0.6 - Retake your Capslock!"

// Tray icon constants
#define ID_TRAYICON          1
#define APPWM_TRAYICON       WM_APP
#define APPWM_NOP            WM_APP + 1

//  Our commands
#define ID_ABOUT             2000
#define ID_EXIT              2001
#define ID_LANG              2002

// Hook stuff
#define WH_KEYBOARD_LL 13
#define LLKHF_INJECTED 0x10
/*typedef struct {
    DWORD vkCode;
    DWORD scanCode;
    DWORD flags;
    DWORD time;
    ULONG_PTR dwExtraInfo;
} KBDLLHOOKSTRUCT, *PKBDLLHOOKSTRUCT;*/

// General constants
#define MAXLEN 1024
#define MAX_LAYOUTS 256
#define MUTEX L"recaps-D3E743A3-E0F9-47f5-956A-CD15C6548789"
#define WINDOWCLASS_NAME L"RECAPS"
#define TITLE L"Recaps"

struct KeyboardLayoutInfo
{
    WCHAR names[MAX_LAYOUTS][MAXLEN];
    HKL   hkls[MAX_LAYOUTS];
    UINT  menuIds[MAX_LAYOUTS];
    BOOL  inUse[MAX_LAYOUTS];
    UINT  count;
    UINT  current;
};

KeyboardLayoutInfo g_keyboardInfo = {0};
BOOL g_modalShown = FALSE;
HHOOK g_hHook = NULL;

// Prototypes
void ShowError(const WCHAR* message);
void PrintDebugString(const char* format, ...);

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int OnTrayIcon(HWND hWnd, WPARAM wParam, LPARAM lParam);
int OnCommand(HWND hWnd, WORD wID, HWND hCtl);
BOOL ShowPopupMenu(HWND hWnd);

void GetKeyboardLayouts(KeyboardLayoutInfo* info);
void LoadConfiguration(KeyboardLayoutInfo* info);
void SaveConfiguration(const KeyboardLayoutInfo* info);

HWND RemoteGetFocus();
HKL SwitchLayout();
LRESULT CALLBACK LowLevelHookProc(int nCode, WPARAM wParam, LPARAM lParam);

///////////////////////////////////////////////////////////////////////////////
// Program's entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	// Prevent from two copies of Recaps from running at the same time
	HANDLE mutex = CreateMutex(NULL, FALSE, MUTEX);
	DWORD result = WaitForSingleObject(mutex, 0);
	if (result == WAIT_TIMEOUT)
	{
		MessageBox(NULL, L"Recaps is already running.", L"Recaps", MB_OK | MB_ICONINFORMATION);
		return 1;
	}

    // Create a fake window to listen to events
    WNDCLASSEX wclx = {0};
    wclx.cbSize         = sizeof( wclx );
    wclx.lpfnWndProc    = &WindowProc;
    wclx.hInstance      = hInstance;
    wclx.lpszClassName  = WINDOWCLASS_NAME;
    RegisterClassEx(&wclx);
    HWND hMessageWindow = CreateWindow(WINDOWCLASS_NAME, 0, 0, 0, 0, 0, 0, HWND_MESSAGE, 0, hInstance, 0);

    // Set hook to capture CapsLock
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelHookProc, GetModuleHandle(NULL), 0);

    // Initialize
    AddTrayIcon(hMessageWindow, 0, APPWM_TRAYICON, IDI_MAINFRAME, TITLE);
    GetKeyboardLayouts(&g_keyboardInfo);
    LoadConfiguration(&g_keyboardInfo);

	// Handle messages
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
        TranslateMessage(&msg);
        DispatchMessage(&msg);
	}

    // Clean up
    RemoveTrayIcon(hMessageWindow, 0);
    UnregisterClass(WINDOWCLASS_NAME, hInstance);
    DestroyWindow(hMessageWindow);
    SaveConfiguration(&g_keyboardInfo);
    UnhookWindowsHookEx(g_hHook);

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Handles events at the window (both hot key and from the tray icon)
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) 
    {
        case APPWM_TRAYICON:
            return OnTrayIcon(hWnd, wParam, lParam);

        case WM_COMMAND:
            return OnCommand(hWnd, LOWORD(wParam), (HWND)lParam);

        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

///////////////////////////////////////////////////////////////////////////////
// Create and display a popup menu when the user right-clicks on the icon
int OnTrayIcon(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(wParam);

    if (g_modalShown == TRUE)
        return 0;

    switch (lParam) 
    {
        case WM_RBUTTONUP:
            // Show the context menu
            ShowPopupMenu(hWnd);
            return 0;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Handles user commands from the menu
int OnCommand(HWND hWnd, WORD wID, HWND hCtl)
{
    UNREFERENCED_PARAMETER(hCtl);

    //  Have a look at the command and act accordingly
    if (wID == ID_EXIT)
    {
        PostMessage( hWnd, WM_CLOSE, 0, 0 );
    }
    else if (wID == ID_ABOUT)
    {
        MessageBox(NULL, HELP_MESSAGE, HELP_TITLE, MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        for (UINT i = 0; i < g_keyboardInfo.count; i++)
        {
            if (g_keyboardInfo.menuIds[i] == wID)
            {
                g_keyboardInfo.inUse[i] = ! (g_keyboardInfo.inUse[i]);
                break;
            }
        }
		SaveConfiguration(&g_keyboardInfo);
    }
    
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Create and display a popup menu when the user right-clicks on the icon
BOOL ShowPopupMenu(HWND hWnd)
{
    HMENU   hPop        = NULL;
    int     i           = 0;
    BOOL    cmd;
    POINT   curpos;

    // Create the menu
    hPop = CreatePopupMenu();
    InsertMenu(hPop, i++, MF_BYPOSITION | MF_STRING, ID_ABOUT, L"Help...");
    InsertMenu(hPop, i++, MF_SEPARATOR, 0, NULL);

    // Add items for the languages
    for (UINT layout = 0; layout < g_keyboardInfo.count; layout++)
    {
        UINT flags = MF_BYPOSITION | MF_STRING;
        if (g_keyboardInfo.inUse[layout])
             flags |= MF_CHECKED;
        InsertMenu(hPop, i++, flags, ID_LANG+layout, g_keyboardInfo.names[layout]);
        g_keyboardInfo.menuIds[layout] = ID_LANG+layout;
    }

    InsertMenu(hPop, i++, MF_SEPARATOR, 0, NULL);
    InsertMenu(hPop, i++, MF_BYPOSITION | MF_STRING, ID_EXIT, L"Exit");

    // Show the menu

    // See http://support.microsoft.com/kb/135788 for the reasons 
    // for the SetForegroundWindow and Post Message trick.
    GetCursorPos(&curpos);
    SetForegroundWindow(hWnd);
    g_modalShown = TRUE;
    cmd = (WORD)TrackPopupMenu(
            hPop, TPM_LEFTALIGN|TPM_RIGHTBUTTON|TPM_RETURNCMD|TPM_NONOTIFY,
            curpos.x, curpos.y, 0, hWnd, NULL
          );
	PostMessage(hWnd, WM_NULL, 0, 0);
    g_modalShown = FALSE;

    // Send a command message to the window to handle the menu item the user chose
    if (cmd)
        SendMessage(hWnd, WM_COMMAND, cmd, 0);

    DestroyMenu(hPop);

    return cmd;
}

///////////////////////////////////////////////////////////////////////////////
// Fills ``info`` with the currently installed keyboard layouts
// Based on http://blogs.msdn.com/michkap/archive/2004/12/05/275231.aspx.
void GetKeyboardLayouts(KeyboardLayoutInfo* info)
{
    memset(info, 0 ,sizeof(KeyboardLayoutInfo));
    info->count = GetKeyboardLayoutList(MAX_LAYOUTS, info->hkls);
    for(UINT i = 0; i < info->count; i++)
    {
        LANGID language = (LANGID)(((UINT)info->hkls[i]) & 0x0000FFFF); // bottom 16 bit of HKL
        LCID locale = MAKELCID(language, SORT_DEFAULT);
        GetLocaleInfo(locale, LOCALE_SLANGUAGE, info->names[i], MAXLEN);
        info->inUse[i] = TRUE;
    }
}

///////////////////////////////////////////////////////////////////////////////
// Load currently active keyboard layouts from the registry
void LoadConfiguration(KeyboardLayoutInfo* info)
{
    HKEY hkey;
    LONG result;

    result = RegOpenKey(HKEY_CURRENT_USER, L"Software\\Recaps", &hkey);

    // Load current isUse value for each language
    if (result == ERROR_SUCCESS)
    {
        for (UINT i = 0; i < info->count; i++)
        {
            DWORD data = 0;
            DWORD length = sizeof(DWORD);
            result = RegQueryValueEx(hkey, info->names[i], 0, NULL, (BYTE*)(&data), &length);
            if (result == ERROR_SUCCESS)
            {
                info->inUse[i] = (BOOL)data;
            }
        }
    }

    RegCloseKey(hkey);
}

///////////////////////////////////////////////////////////////////////////////
// Saves currently active keyboard layouts to the registry
void SaveConfiguration(const KeyboardLayoutInfo* info)
{
    HKEY hkey;
    LONG result;

    result = RegOpenKey(HKEY_CURRENT_USER, L"Software\\Recaps", &hkey);

    if (result != ERROR_SUCCESS)
    {
        result = RegCreateKey(HKEY_CURRENT_USER, L"Software\\Recaps", &hkey);
    }

    // Save current isUse value for each language
    if (result == ERROR_SUCCESS)
    {
        for (UINT i = 0; i < info->count; i++)
        {
            DWORD data = info->inUse[i];
            RegSetValueEx(hkey, info->names[i], 0, REG_DWORD, (CONST BYTE*)(&data), sizeof(DWORD));
        }
    }

    RegCloseKey(hkey);
}

///////////////////////////////////////////////////////////////////////////////
// Finds out which window has the focus
HWND RemoteGetFocus()
{
    HWND hwnd = GetForegroundWindow();
	DWORD remoteThreadId = GetWindowThreadProcessId(hwnd, NULL);
	DWORD currentThreadId = GetCurrentThreadId();
	AttachThreadInput(remoteThreadId, currentThreadId, TRUE);
	HWND focused = GetFocus();
	AttachThreadInput(remoteThreadId, currentThreadId, FALSE);
	return focused;
}

///////////////////////////////////////////////////////////////////////////////
// Returns the current layout in the active window
HKL GetCurrentLayout()
{
	HWND hwnd = RemoteGetFocus();
    DWORD threadId = GetWindowThreadProcessId(hwnd, NULL);
    return GetKeyboardLayout(threadId);
}

///////////////////////////////////////////////////////////////////////////////
// Switches the current language
HKL SwitchLayout()
{
	HWND hwnd = RemoteGetFocus();
	HKL currentLayout = GetCurrentLayout();

    // Find the current keyboard layout's index
	UINT i;
    for (i = 0; i < g_keyboardInfo.count; i++)
    {
        if (g_keyboardInfo.hkls[i] == currentLayout)
            break;
    }
    UINT currentLanguageIndex = i;
    
    // Find the next active layout
    BOOL found = FALSE;
    UINT newLanguage = currentLanguageIndex;
    for (UINT i = 0; i < g_keyboardInfo.count; i++)
    {
        newLanguage = (newLanguage + 1) % g_keyboardInfo.count;
        if (g_keyboardInfo.inUse[newLanguage])
        {
            found = TRUE;
            break;
        }
    }

    // Activate the selected language
    if (found)
    {
        g_keyboardInfo.current = newLanguage;
        PostMessage(hwnd, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)(g_keyboardInfo.hkls[g_keyboardInfo.current]));
        #ifdef _DEBUG
			PrintDebugString("Language set to %S", g_keyboardInfo.names[g_keyboardInfo.current]);
        #endif
		return g_keyboardInfo.hkls[g_keyboardInfo.current];
    }

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Selects the entire current line and converts it to the current kwyboard layout
void SwitchAndConvert(void*)
{
	SendKeyCombo(VK_CONTROL, 'A', TRUE);
	HKL sourceLayout = GetCurrentLayout();
	HKL targetLayout = SwitchLayout();
	ConvertSelectedTextInActiveWindow(sourceLayout, targetLayout);
}

///////////////////////////////////////////////////////////////////////////////
// A LowLevelHookProc implementation that captures the CapsLock key
LRESULT CALLBACK LowLevelHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode < 0) return CallNextHookEx(g_hHook, nCode, wParam, lParam);

    KBDLLHOOKSTRUCT* data = (KBDLLHOOKSTRUCT*)lParam;

    BOOL ctrl =  (GetKeyState(VK_CONTROL) & 0x80000000) > 0;
    BOOL caps =  data->vkCode == VK_CAPITAL && wParam == WM_KEYDOWN;

    // ignore injected keystrokes
    if ((data->flags & LLKHF_INJECTED) == 0)
    {
        // Handle CapsLock - only switch current layout
        if (caps && !ctrl)
		{
			SwitchLayout();
			return 1;
		}
        // Handle Ctrl-CapsLock - switch current layout and convert text in current field
		else if (caps && ctrl)
        {
			// We start SwitchLayoutAndConvertSelected in another thread since it simulates 
			// keystrokes to copy and paste the teset which call back into this hook.
			// That isn't good..
			_beginthread(SwitchAndConvert, 0, NULL);
            return 1; // prevent windows from handling the keystroke
        }
    }

    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}
