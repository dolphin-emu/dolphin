#pragma once

#include <string>
#include <windows.h>

namespace EmuWindow
{

HWND GetWnd();
HWND GetParentWnd();
HWND Create(HWND hParent, HINSTANCE hInstance, const TCHAR *title);
void Show();
void Close();
void SetSize(int displayWidth, int displayHeight);
bool IsSizing();
void OSDMenu(WPARAM wParam);
void SetWindowText(const std::string& text);

}
