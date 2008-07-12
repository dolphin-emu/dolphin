#include <windows.h>
#include <vector>
#include "DialogManager.h"

typedef std::vector <HWND> WindowList;
WindowList dialogs;

void DialogManager::AddDlg(HWND hDialog)
{
	dialogs.push_back(hDialog);
}

bool DialogManager::IsDialogMessage(LPMSG message)
{
	WindowList::iterator iter;
	for (iter=dialogs.begin(); iter!=dialogs.end(); iter++)
	{
		if (::IsDialogMessage(*iter,message))
			return true;
	}
	return false;
}

void DialogManager::EnableAll(BOOL enable)
{
	WindowList::iterator iter;
	for (iter=dialogs.begin(); iter!=dialogs.end(); iter++)
		EnableWindow(*iter,enable); 
}
