#include "stdafx.h"
#include "trayicon.h"

void AddTrayIcon(HWND hWnd, UINT uID, UINT uCallbackMsg, UINT uIcon, LPTSTR pszToolTip)
{
    NOTIFYICONDATA  nid     = {0};
    nid.cbSize              = sizeof(nid);
    nid.hWnd                = hWnd;
    nid.uID                 = uID;
    nid.uFlags              = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage    = uCallbackMsg;
    nid.hIcon               = LoadSmallIcon(GetModuleHandle(NULL), uIcon);
    wcscpy_s(nid.szTip, sizeof(WCHAR)*64, pszToolTip);
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void ModifyTrayIcon( HWND hWnd, UINT uID, UINT uIcon, LPTSTR pszToolTip )
{
    NOTIFYICONDATA  nid = {0};
    nid.cbSize  = sizeof(nid);
    nid.hWnd    = hWnd;
    nid.uID     = uID;

    if (uIcon != (UINT)-1) 
    {
        nid.hIcon = LoadSmallIcon(GetModuleHandle( NULL ), uIcon);
        nid.uFlags |= NIF_ICON;
    }

    if (pszToolTip) 
    {
        wcscpy_s(nid.szTip, sizeof(WCHAR)*64, pszToolTip);
        nid.uFlags |= NIF_TIP;
    }

    if (uIcon != (UINT)-1 || pszToolTip)
        Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void RemoveTrayIcon(HWND hWnd, UINT uID)
{
    NOTIFYICONDATA nid = {0};
    nid.cbSize  = sizeof( nid );
    nid.hWnd    = hWnd;
    nid.uID     = uID;
    Shell_NotifyIcon( NIM_DELETE, &nid );
}

HICON LoadSmallIcon(HINSTANCE hInstance, UINT uID)
{
    return (HICON)LoadImage(hInstance, MAKEINTRESOURCE(uID), IMAGE_ICON, 16, 16, 0);
}
