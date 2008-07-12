// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <windowsx.h>
#include "resource.h"

#include "DirectInputBase.h"
#include "ConfigDlg.h"
#define NUMCONTROLS 23
int m_buttonResources[NUMCONTROLS] =
{
	IDC_SETMAINLEFT,
	IDC_SETMAINUP,
	IDC_SETMAINRIGHT,
	IDC_SETMAINDOWN,
	IDC_SETSUBLEFT,
	IDC_SETSUBUP,
	IDC_SETSUBRIGHT,
	IDC_SETSUBDOWN,
	IDC_SETDPADLEFT,
	IDC_SETDPADUP,
	IDC_SETDPADRIGHT,
	IDC_SETDPADDOWN,
	IDC_SETA,
	IDC_SETB,
	IDC_SETX,
	IDC_SETY,
	IDC_SETZ,
	IDC_SETL,
	IDC_SETR,
	IDC_SETSTART,
	//	CTL_HALFMAIN,
	//	CTL_HALFSUB,
	//	CTL_HALFTRIGGER,
	//	NUMCONTROLS
};

extern int keyForControl[NUMCONTROLS];
extern bool g_rumbleEnable;

LRESULT CConfigDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_dinput.Init(m_hWnd);

	CenterWindow(GetParent());

	for (int i = 0; i < NUMCONTROLS - 3; i++)
	{
		CButton button = GetDlgItem(m_buttonResources[i]);
		SetButtonText(button, keyForControl[i]);
	}

	m_hWaitForKeyButton = NULL;
	CheckDlgButton(IDC_RUMBLE1, g_rumbleEnable);
	SetTimer(1, 50, 0);
	return(TRUE);
}


LRESULT CConfigDlg::OnCommand(UINT /*uMsg*/, WPARAM _wParam, LPARAM _lParam, BOOL& _bHandled)
{
	// we have not handled it
	_bHandled = FALSE;

	// check if it is a key
	for (int i = 0; i < NUMCONTROLS; i++)
	{
		if (m_buttonResources[i] == LOWORD(_wParam))
		{
			m_iKeyWaitingFor = i;
			m_hWaitForKeyButton = GetDlgItem(m_buttonResources[i]);

			CButton tmpButton = m_hWaitForKeyButton;
			tmpButton.SetWindowText("Press Key");

			_bHandled = TRUE; // yeah we have handled it
			break;
		}
	}

	g_rumbleEnable = Button_GetCheck(GetDlgItem(IDC_RUMBLE1)) ? true : false;
	return(TRUE);
}


LRESULT CConfigDlg::OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (m_hWaitForKeyButton != NULL)
	{
		m_dinput.Read();

		for (int i = 0; i < 255; i++)
		{
			if (m_dinput.diks[i])
			{
				keyForControl[m_iKeyWaitingFor] = i;
				CButton tmpButton = m_hWaitForKeyButton;
				SetButtonText(tmpButton, keyForControl[m_iKeyWaitingFor]);

				m_hWaitForKeyButton = NULL;
				break;
			}
		}
	}

	return(TRUE);
}


LRESULT CConfigDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_dinput.Free();

	EndDialog(wID);
	return(0);
}


void CConfigDlg::SetButtonText(CButton& _rButton, int _key)
{
	char szTemp[64];

	switch (_key)
	{
	    case DIK_LEFT:
		    strcpy(szTemp, "Left");
		    break;

	    case DIK_UP:
		    strcpy(szTemp, "Up");
		    break;

	    case DIK_RIGHT:
		    strcpy(szTemp, "Right");
		    break;

	    case DIK_DOWN:
		    strcpy(szTemp, "Down");
		    break;

	    case DIK_HOME:
		    strcpy(szTemp, "Home");
		    break;

	    case DIK_END:
		    strcpy(szTemp, "End");
		    break;

	    case DIK_INSERT:
		    strcpy(szTemp, "Ins");
		    break;

	    case DIK_DELETE:
		    strcpy(szTemp, "Del");
		    break;

	    case DIK_PGUP:
		    strcpy(szTemp, "PgUp");
		    break;

	    case DIK_PGDN:
		    strcpy(szTemp, "PgDn");
		    break;

	    default:
		    GetKeyNameText(_key << 16, szTemp, 64);
		    break;
	}

	_rButton.SetWindowText(szTemp);
}


