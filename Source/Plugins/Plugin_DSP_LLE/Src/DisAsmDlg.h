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

#include "Globals.h"

#include <list>
#include <algorithm>

#include "../res/resource.h"
#include "DisAsmListView.h"
#include "RegisterDlg.h"

class CDisAsmDlg
	: public CDialogImpl<CDisAsmDlg>, public CUpdateUI<CDisAsmDlg>, public CDialogResize<CDisAsmDlg>
{
	public:

		CDisAsmDlg();

		enum { IDD = IDD_DISASMDLG };

		virtual BOOL PreTranslateMessage(MSG* pMsg);
		virtual BOOL OnIdle();


		BEGIN_UPDATE_UI_MAP(CDisAsmDlg)
		END_UPDATE_UI_MAP()

		BEGIN_DLGRESIZE_MAP(CDisAsmDlg)
			DLGRESIZE_CONTROL(IDR_MAINFRAME, DLSZ_SIZE_X | DLSZ_SIZE_Y)
		END_DLGRESIZE_MAP()

		BEGIN_MSG_MAP(CDisAsmDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_ID_HANDLER(ID_STEP, OnStep)
		COMMAND_ID_HANDLER(ID_GO, OnGo)
		COMMAND_ID_HANDLER(ID_SHOW_REGISTER, OnShowRegisters)
		NOTIFY_CODE_HANDLER(NM_CLICK, OnDblClick)
		NOTIFY_CODE_HANDLER(NM_RETURN, OnDblClick)
		NOTIFY_CODE_HANDLER(NM_RCLICK, OnRClick)
		NOTIFY_CODE_HANDLER(NM_CUSTOMDRAW, OnCustomDraw)
		NOTIFY_HANDLER(IDC_DISASM_LIST, LVN_ITEMCHANGED, OnLvnItemchangedDisasmList)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		CHAIN_MSG_MAP(CDialogResize<CDisAsmDlg>)

		END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/);
		LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL & /*bHandled*/);

		LRESULT OnStep(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL & /*bHandled*/);
		LRESULT OnGo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL & /*bHandled*/);
		LRESULT OnShowRegisters(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL & /*bHandled*/);
		LRESULT OnCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& _bHandled);

		LRESULT OnDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT OnRClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);

		void CloseDialog(int nVal);

		bool CanDoStep();

		void DebugBreak();


	private:

		enum EColumns
		{
			ColumnBP = 0,
			ColumnFunction = 1,
			ColumnAddress = 2,
			ColumnMenmomic = 3,
			ColumnOpcode = 4,
			ColumnExt = 5,
			ColumnParameter = 6
		};

		enum EState
		{
			PAUSE,
			STEP,
			RUN,
			RUN_START // ignores breakpoints and switches after one step to RUN
		};
		EState m_State;


		CListViewCtrl m_DisAsmListViewCtrl;
		CRegisterDlg m_RegisterDlg;
		//CWindow GroupLeft
		CStatic GroupLeft;

		uint64 m_CachedStepCounter;
		uint16 m_CachedCR;
		uint32 m_CachedUCodeCRC;

		typedef std::list<uint16>CBreakPointList;
		CBreakPointList m_BreakPoints;

		// break point handling
		bool IsBreakPoint(uint16 _Address);
		void ToggleBreakPoint(uint16 _Address);
		void RemoveBreakPoint(uint16 _Address);
		void AddBreakPoint(uint16 _Address);
		void ClearBreakPoints();


		// update dialog
		void UpdateDisAsmListView();
		void UpdateRegisterFlags();
		void UpdateSymbolMap();
		void UpdateButtonTexts();

		void RedrawDisAsmListView();
		void RebuildDisAsmListView();


		struct SSymbol
		{
			uint32 AddressStart;
			uint32 AddressEnd;
			std::string Name;

			SSymbol(uint32 _AddressStart = 0, uint32 _AddressEnd = 0, char* _Name = NULL)
				: AddressStart(_AddressStart)
				, AddressEnd(_AddressEnd)
				, Name(_Name)
			{}
		};
		typedef std::map<uint16, SSymbol>CSymbolMap;
		CSymbolMap m_SymbolMap;

		void AddSymbol(uint16 _AddressStart, uint16 _AddressEnd, char* _Name);
		bool LoadSymbolMap(const char* _pFileName);
		DWORD FindColor(uint16 _Address);
		void UpdateDialog();

		static int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
public:
	LRESULT OnLvnItemchangedDisasmList(int /*idCtrl*/, LPNMHDR pNMHDR, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
};
