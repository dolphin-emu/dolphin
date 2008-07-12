#pragma once

#include <windows.h>

class DialogManager
{
public:
	static void AddDlg(HWND hDialog);
	static bool IsDialogMessage(LPMSG message);
	static void EnableAll(BOOL enable);
};

