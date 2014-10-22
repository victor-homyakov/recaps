#pragma once

//  Prototypes
void    AddTrayIcon( HWND hWnd, UINT uID, UINT uCallbackMsg, UINT uIcon, LPTSTR pszToolTip );
void    RemoveTrayIcon( HWND hWnd, UINT uID);
void    ModifyTrayIcon( HWND hWnd, UINT uID, UINT uIcon, LPTSTR pszToolTip );
HICON   LoadSmallIcon( HINSTANCE hInstance, UINT uID );
