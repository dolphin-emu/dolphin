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

#include "stdafx.h"
#include "../res/resource.h"
#include "DisAsmDlg.h"

#include "gdsp_memory.h"
#include "gdsp_interpreter.h"
#include "disassemble.h"
#include "RegSettings.h"

CDisAsmDlg::CDisAsmDlg()
	: m_CachedStepCounter(-1)
	, m_CachedCR(-1)
	, m_State(RUN)
	, m_CachedUCodeCRC(-1)
{}


BOOL CDisAsmDlg::PreTranslateMessage(MSG* pMsg)
{
	return(IsDialogMessage(pMsg));
}


BOOL CDisAsmDlg::OnIdle()
{
	return(FALSE);
}


LRESULT CDisAsmDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CWindowSettings ws;

	if (ws.Load("Software\\Dolphin\\DSP", "DisAsm"))
	{
		ws.ApplyTo(CWindow(m_hWnd), SW_SHOW);
	}

	m_DisAsmListViewCtrl.m_hWnd = GetDlgItem(IDC_DISASM_LIST);

	UIAddChildWindowContainer(m_hWnd);

	m_DisAsmListViewCtrl.AddColumn(_T("BP"),     ColumnBP);
	m_DisAsmListViewCtrl.AddColumn(_T("Function"),  ColumnFunction);
	m_DisAsmListViewCtrl.AddColumn(_T("Address"),    ColumnAddress);
	m_DisAsmListViewCtrl.AddColumn(_T("Menmomic"),   ColumnMenmomic);
	m_DisAsmListViewCtrl.AddColumn(_T("Opcode"), ColumnOpcode);
	m_DisAsmListViewCtrl.AddColumn(_T("Ext"),        ColumnExt);
	m_DisAsmListViewCtrl.AddColumn(_T("Parameter"), ColumnParameter);

	m_DisAsmListViewCtrl.SetColumnWidth(ColumnBP,           25);
	m_DisAsmListViewCtrl.SetColumnWidth(ColumnFunction,     160);
	m_DisAsmListViewCtrl.SetColumnWidth(ColumnAddress,      55);
	m_DisAsmListViewCtrl.SetColumnWidth(ColumnMenmomic,     55);
	m_DisAsmListViewCtrl.SetColumnWidth(ColumnOpcode,       60);
	m_DisAsmListViewCtrl.SetColumnWidth(ColumnExt,          40);
	m_DisAsmListViewCtrl.SetColumnWidth(ColumnParameter,    500);

	m_DisAsmListViewCtrl.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

	m_RegisterDlg.Create(m_hWnd);

	UpdateDialog();

	return(TRUE);
}


LRESULT CDisAsmDlg::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CWindowSettings ws;
	ws.GetFrom(CWindow(m_hWnd));
	ws.Save("Software\\Dolphin\\DSP", "DisAsm");

	return(0);
}


LRESULT CDisAsmDlg::OnStep(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_State = STEP;

	UpdateButtonTexts();
	return(0);
}


LRESULT CDisAsmDlg::OnGo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if ((m_State == RUN) || (m_State == RUN_START))
	{
		m_State = PAUSE;
	}
	else
	{
		m_State = RUN_START;
	}

	UpdateButtonTexts();
	return(0);
}


LRESULT CDisAsmDlg::OnShowRegisters(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_RegisterDlg.IsWindowVisible())
	{
		m_RegisterDlg.ShowWindow(SW_HIDE);
	}
	else
	{
		m_RegisterDlg.ShowWindow(SW_SHOW);
	}

	UpdateButtonTexts();
	return(0);
}


LRESULT CDisAsmDlg::OnDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	int Index = m_DisAsmListViewCtrl.GetSelectedIndex();

	if (Index != -1)
	{
		uint16 SelectedPC = static_cast<uint16>(m_DisAsmListViewCtrl.GetItemData(Index));
		ToggleBreakPoint(SelectedPC);
	}

	RedrawDisAsmListView();
	return(0);
}


LRESULT CDisAsmDlg::OnRClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	int Index = m_DisAsmListViewCtrl.GetSelectedIndex();

	if (Index != -1)
	{
		uint16 SelectedPC = static_cast<uint16>(m_DisAsmListViewCtrl.GetItemData(Index));
		g_dsp.pc = SelectedPC;
	}

	RedrawDisAsmListView();
	return(0);
}


void CDisAsmDlg::CloseDialog(int nVal)
{
	DestroyWindow();
	::PostQuitMessage(nVal);
}


int CALLBACK CDisAsmDlg::CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
	return(lParam1 > lParam2);
}


void CDisAsmDlg::RebuildDisAsmListView()
{
	m_DisAsmListViewCtrl.ShowWindow(SW_HIDE);
	m_DisAsmListViewCtrl.DeleteAllItems();

	char Buffer[256];
	gd_globals_t gdg;

	if (g_dsp.pc & 0x8000)
	{
		gdg.binbuf = g_dsp.irom;
	}
	else
	{
		gdg.binbuf = g_dsp.iram;
	}

	gdg.buffer = Buffer;
	gdg.buffer_size   = 256;
	gdg.ext_separator = (char)0xff;

	gdg.show_pc      = false;
	gdg.show_hex     = false;
	gdg.print_tabs   = true;
	gdg.decode_names = true;
	gdg.decode_registers = true;

	for (gdg.pc = 0; gdg.pc < DSP_IROM_SIZE;)
	{
		uint16 CurrentPC = gdg.pc;

		if (g_dsp.pc & 0x8000)
		{
			CurrentPC |= 0x8000;
		}

		char Temp[256];
		sprintf_s(Temp, 256, "0x%04x", CurrentPC);

		char Temp2[256];
		sprintf_s(Temp2, 256, "0x%04x", dsp_imem_read(CurrentPC));

		char* pOpcode = gd_dis_opcode(&gdg);
		const char* pParameter = NULL;
		const char* pExtension = NULL;

		size_t WholeString = strlen(pOpcode);

		for (size_t i = 0; i < WholeString; i++)
		{
			if (pOpcode[i] == (char)0xff)
			{
				pOpcode[i] = 0x00;
				pExtension = &pOpcode[i + 1];
			}

			if (pOpcode[i] == 0x09)
			{
				pOpcode[i] = 0x00;
				pParameter = &pOpcode[i + 1];
			}
		}


		const char* pFunctionName = NULL;

		if (m_SymbolMap.find(CurrentPC) != m_SymbolMap.end())
		{
			pFunctionName = m_SymbolMap[CurrentPC].Name.c_str();
		}

		int Item = m_DisAsmListViewCtrl.AddItem(0, ColumnBP, _T(" "));
		m_DisAsmListViewCtrl.AddItem(Item, ColumnFunction, pFunctionName);
		m_DisAsmListViewCtrl.AddItem(Item, ColumnAddress, Temp);
		m_DisAsmListViewCtrl.AddItem(Item, ColumnMenmomic, Temp2);
		m_DisAsmListViewCtrl.AddItem(Item, ColumnOpcode, pOpcode);
		m_DisAsmListViewCtrl.AddItem(Item, ColumnExt, pExtension);

		if (!_stricmp(pOpcode, "CALL"))
		{
			uint32 FunctionAddress = -1;
			sscanf(pParameter, "0x%04x", &FunctionAddress);

			if (m_SymbolMap.find(FunctionAddress) != m_SymbolMap.end())
			{
				pParameter = m_SymbolMap[FunctionAddress].Name.c_str();
			}
		}

		m_DisAsmListViewCtrl.AddItem(Item, ColumnParameter, pParameter);

		m_DisAsmListViewCtrl.SetItemData(Item, CurrentPC);
	}

	m_DisAsmListViewCtrl.SortItems(CompareFunc, (LPARAM) this);

	m_DisAsmListViewCtrl.ShowWindow(SW_SHOW);
}


void CDisAsmDlg::UpdateDisAsmListView()
{
	if (g_dsp.dram == NULL)
	{
		return;
	}

	// check if we have to rebuild the list view
	if (m_DisAsmListViewCtrl.GetItemCount() == 0)
	{
		RebuildDisAsmListView();
	}
	else
	{
		uint16 FirstPC = static_cast<uint16>(m_DisAsmListViewCtrl.GetItemData(0));

		if ((FirstPC & 0x8000) != (g_dsp.pc & 0x8000))
		{
			RebuildDisAsmListView();
		}
	}

	if (m_CachedStepCounter == g_dsp.step_counter)
	{
		return;
	}

	// show PC
	for (int i = 0; i < m_DisAsmListViewCtrl.GetItemCount(); i++)
	{
		if (m_DisAsmListViewCtrl.GetItemData(i) == g_dsp.pc)
		{
			m_DisAsmListViewCtrl.EnsureVisible(i - 5, FALSE);
			m_DisAsmListViewCtrl.EnsureVisible(i + 14, FALSE);
			break;
		}
	}

	m_CachedStepCounter = g_dsp.step_counter;

	RedrawDisAsmListView();

	m_RegisterDlg.UpdateRegisterListView();
}


void CDisAsmDlg::UpdateSymbolMap()
{
	if (g_dsp.dram == NULL)
	{
		return;
	}

	if (m_CachedUCodeCRC != g_dsp.iram_crc)
	{
		// load symbol map (if there is one)
		m_CachedUCodeCRC = g_dsp.iram_crc;
		char FileName[MAX_PATH];
		sprintf(FileName, "maps\\DSP_%08x.map", m_CachedUCodeCRC);
		LoadSymbolMap(FileName);

		// rebuild the disasm
		RebuildDisAsmListView();
	}
}


void CDisAsmDlg::UpdateRegisterFlags()
{
	if (m_CachedCR == g_dsp.cr)
	{
		return;
	}

	CButton ButtonAssertInt(GetDlgItem(IDC_ASSERT_INT));
	ButtonAssertInt.SetCheck(g_dsp.cr & 0x02 ? BST_CHECKED : BST_UNCHECKED);

	CButton ButtonReset(GetDlgItem(IDC_HALT));
	ButtonReset.SetCheck(g_dsp.cr & 0x04 ? BST_CHECKED : BST_UNCHECKED);

	CButton ButtonInit(GetDlgItem(IDC_INIT));
	ButtonInit.SetCheck(g_dsp.cr & 0x800 ? BST_CHECKED : BST_UNCHECKED);

	m_CachedCR = g_dsp.cr;
}


bool CDisAsmDlg::CanDoStep()
{
	UpdateSymbolMap(); // update the symbols all the time because there a script cmds like bps

	switch (m_State)
	{
	    case RUN_START:
		    m_State = RUN;
		    return(true);

	    case RUN:

		    if (IsBreakPoint(g_dsp.pc))
		    {
			    UpdateDialog();
			    m_State = PAUSE;
			    return(false);
		    }

		    return(true);

	    case PAUSE:
		    UpdateDialog();
		    return(false);

	    case STEP:
		    UpdateDialog();
		    m_State = PAUSE;
		    return(true);
	}

	return(false);
}


void CDisAsmDlg::DebugBreak()
{
	m_State = PAUSE;
}


void CDisAsmDlg::UpdateButtonTexts()
{
	// go button
	{
		CButton Button(GetDlgItem(ID_GO));

		switch (m_State)
		{
		    case RUN_START:
		    case RUN:
			    Button.SetWindowText("Pause");
			    break;

		    case PAUSE:
		    case STEP:
			    Button.SetWindowText("Go");
			    break;
		}
	}

	// show register
	{
		CButton Button(GetDlgItem(ID_SHOW_REGISTER));

		if (m_RegisterDlg.IsWindowVisible())
		{
			Button.SetWindowText("Hide Regs");
		}
		else
		{
			Button.SetWindowText("Show Regs");
		}
	}
}


bool CDisAsmDlg::IsBreakPoint(uint16 _Address)
{
	return(std::find(m_BreakPoints.begin(), m_BreakPoints.end(), _Address) != m_BreakPoints.end());
}


void CDisAsmDlg::ToggleBreakPoint(uint16 _Address)
{
	if (IsBreakPoint(_Address))
	{
		RemoveBreakPoint(_Address);
	}
	else
	{
		AddBreakPoint(_Address);
	}
}


void CDisAsmDlg::RemoveBreakPoint(uint16 _Address)
{
	CBreakPointList::iterator itr = std::find(m_BreakPoints.begin(), m_BreakPoints.end(), _Address);

	if (itr != m_BreakPoints.end())
	{
		m_BreakPoints.erase(itr);
	}
}


void CDisAsmDlg::AddBreakPoint(uint16 _Address)
{
	CBreakPointList::iterator itr = std::find(m_BreakPoints.begin(), m_BreakPoints.end(), _Address);

	if (itr == m_BreakPoints.end())
	{
		m_BreakPoints.push_back(_Address);
	}
}


void CDisAsmDlg::ClearBreakPoints()
{
	m_BreakPoints.clear();
}


LRESULT CDisAsmDlg::OnCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& _bHandled)
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

		    uint16 CurrentAddress = static_cast<uint16>(m_DisAsmListViewCtrl.GetItemData((int)pLVCD->nmcd.dwItemSpec));
		    pLVCD->clrTextBk = FindColor(CurrentAddress);

		    if (CurrentAddress == g_dsp.pc)
		    {
			    pLVCD->clrTextBk = RGB(96,  192,  128);
		    }

		    switch (pLVCD->iSubItem)
		    {
			case 0x00:
			{
				if (IsBreakPoint(CurrentAddress))
				{
					pLVCD->clrTextBk = RGB(255,  64,  64);
				}
			}
				break;

			default:
				break;
		    }
	    }
	}

	return(result);
}


void CDisAsmDlg::RedrawDisAsmListView()
{
	::InvalidateRect(m_DisAsmListViewCtrl.m_hWnd, NULL, FALSE);
}


bool CDisAsmDlg::LoadSymbolMap(const char* _pFileName)
{
	m_SymbolMap.clear();

	FILE* pFile = fopen(_pFileName, "r");

	if (!pFile)
	{
		return(false);
	}

	char Name[1024];
	uint32 AddressStart, AddressEnd;

	while (!feof(pFile))
	{
		char line[512];
		fgets(line, 511, pFile);

		if (strlen(line) < 2)
		{
			continue;
		}

		// check for comment
		if (line[0] == '.')
		{
			continue;
		}

		// clear all breakpoints
		if (line[0] == 'C')
		{
			ClearBreakPoints();
			continue;
		}

		// add breakpoint
		if (line[0] == 'B')
		{
			sscanf(line, "B %04x", &AddressStart);
			AddBreakPoint(static_cast<uint16>(AddressStart));
			continue;
		}

		// default add new symbol
		sscanf(line, "%04x %04x %s", &AddressStart, &AddressEnd, Name);

		if (m_SymbolMap.find(AddressStart) == m_SymbolMap.end())
		{
			m_SymbolMap.insert(std::pair<uint16, SSymbol>(AddressStart, SSymbol(AddressStart, AddressEnd, Name)));
		}
		else
		{
			m_SymbolMap[AddressStart] = SSymbol(AddressStart, AddressEnd, Name);
		}
	}

	fclose(pFile);

	return(true);
}


DWORD CDisAsmDlg::FindColor(uint16 _Address)
{
	size_t Color = 0;
	static int Colors[6] = {0xC0FFFF, 0xFFE0C0, 0xC0C0FF, 0xFFC0FF, 0xC0FFC0, 0xFFFFC0};

	for (CSymbolMap::const_iterator itr = m_SymbolMap.begin(); itr != m_SymbolMap.end(); itr++)
	{
		const SSymbol& rSymbol = itr->second;

		if ((rSymbol.AddressStart <= _Address) && (_Address <= rSymbol.AddressEnd))
		{
			return(Colors[Color % 6]);
		}

		Color++;
	}

	return(GetSysColor(COLOR_3DLIGHT));
}


void CDisAsmDlg::UpdateDialog()
{
	UpdateSymbolMap();
	UpdateDisAsmListView();
//	UpdateButtonTexts();
	UpdateRegisterFlags();
}
