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

#include "../res/resource.h"
#include "RegisterDlg.h"

#include "disassemble.h"
#include "gdsp_interpreter.h"
#include "RegSettings.h"

CRegisterDlg::CRegisterDlg()
	: m_CachedCounter(-1)
{}


BOOL CRegisterDlg::PreTranslateMessage(MSG* pMsg)
{
	return(IsDialogMessage(pMsg));
}


BOOL CRegisterDlg::OnIdle()
{
	return(FALSE);
}


LRESULT CRegisterDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CWindowSettings ws;

	if (ws.Load("Software\\Dolphin\\DSP", "Register"))
	{
		ws.ApplyTo(CWindow(m_hWnd), SW_SHOW);
	}

	m_RegisterListViewCtrl.m_hWnd = GetDlgItem(IDC_DISASM_LIST);

	UIAddChildWindowContainer(m_hWnd);

	m_RegisterListViewCtrl.AddColumn(_T("General"), 0);
	m_RegisterListViewCtrl.AddColumn(_T(" "),        1);
	m_RegisterListViewCtrl.AddColumn(_T("Special"),  2);
	m_RegisterListViewCtrl.AddColumn(_T("0"),        3);

	m_RegisterListViewCtrl.SetColumnWidth(0, 50);
	m_RegisterListViewCtrl.SetColumnWidth(1, 100);
	m_RegisterListViewCtrl.SetColumnWidth(2, 60);
	m_RegisterListViewCtrl.SetColumnWidth(3, 100);

	m_RegisterListViewCtrl.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_RegisterListViewCtrl.SetTextBkColor(GetSysColor(COLOR_3DLIGHT));


	for (uint16 i = 0; i < 16; i++)
	{
		// 0-15
		int Item = m_RegisterListViewCtrl.AddItem(0, 0, gd_dis_get_reg_name(i));

		// 16-31
		m_RegisterListViewCtrl.AddItem(Item, 2, gd_dis_get_reg_name(16 + i));

		// just for easy sort
		m_RegisterListViewCtrl.SetItemData(Item, i);
	}

	m_RegisterListViewCtrl.SortItems(CompareFunc, (LPARAM) this);

	UpdateRegisterListView();

	return(TRUE);
}


LRESULT CRegisterDlg::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CWindowSettings ws;
	ws.GetFrom(CWindow(m_hWnd));
	ws.Save("Software\\Dolphin\\DSP", "Register");

	return(0);
}


void CRegisterDlg::UpdateRegisterListView()
{
	if (m_CachedCounter == g_dsp.step_counter)
	{
		return;
	}

	m_CachedCounter = g_dsp.step_counter;

	char Temp[256];

	for (uint16 i = 0; i < 16; i++)
	{
		// 0-15
		if (m_CachedRegs[i] != g_dsp.r[i])
		{
			m_CachedRegHasChanged[i] = true;
		}
		else
		{
			m_CachedRegHasChanged[i] = false;
		}

		m_CachedRegs[i] = g_dsp.r[i];

		sprintf_s(Temp, 256, "0x%04x", g_dsp.r[i]);
		m_RegisterListViewCtrl.SetItemText(i, 1, Temp);

		// 16-31
		if (m_CachedRegs[16 + i] != g_dsp.r[16 + i])
		{
			m_CachedRegHasChanged[16 + i] = true;
		}
		else
		{
			m_CachedRegHasChanged[16 + i] = false;
		}

		m_CachedRegs[16 + i] = g_dsp.r[16 + i];

		sprintf_s(Temp, 256, "0x%04x", g_dsp.r[16 + i]);
		m_RegisterListViewCtrl.SetItemText(i, 3, Temp);
	}
}


int CALLBACK CRegisterDlg::CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	return(lParam1 > lParam2);
}


LRESULT CRegisterDlg::OnCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& _bHandled)
{
	int result = CDRF_DODEFAULT;

	NMLVCUSTOMDRAW* pLVCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pnmh);

	switch (pLVCD->nmcd.dwDrawStage)
	{
	    case CDDS_PREPAINT:
		    result = CDRF_NOTIFYITEMDRAW;
		    break;

	    case CDDS_ITEMPREPAINT:
		    result = CDRF_NOTIFYSUBITEMDRAW;
		    break;

	    case (CDDS_ITEMPREPAINT | CDDS_SUBITEM):
	    {
		    pLVCD->nmcd.uItemState &= ~(CDIS_SELECTED | CDIS_FOCUS);

		    int Offset = static_cast<int>(m_RegisterListViewCtrl.GetItemData((int)pLVCD->nmcd.dwItemSpec));

		    size_t Register = -1;

		    if (pLVCD->iSubItem == 1)
		    {
			    Register = Offset;
		    }
		    else if (pLVCD->iSubItem == 3)
		    {
			    Register = Offset + 16;
		    }

		    if (Register != -1)
		    {
			    if (m_CachedRegHasChanged[Register])
			    {
				    pLVCD->clrTextBk = RGB(0xFF,  192,  192);
			    }
			    else
			    {
				    pLVCD->clrTextBk = RGB(0xF0,  0xF0,  0xF0);
			    }
		    }
		    else
		    {
			    pLVCD->clrTextBk = RGB(192,  224,  192);
		    }


		    // uint16 CurrentAddress = static_cast<uint16>(m_DisAsmListViewCtrl.GetItemData((int)pLVCD->nmcd.dwItemSpec));
	    }
		    break;
	}

	return(result);
}
