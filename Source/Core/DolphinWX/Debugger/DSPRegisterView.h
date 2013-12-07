// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef __DSPREGISTERVIEW_h__
#define __DSPREGISTERVIEW_h__

#include <wx/grid.h>


class CDSPRegTable : public wxGridTableBase
{
private:
	u64 m_CachedCounter;
	u16 m_CachedRegs[32];
	bool m_CachedRegHasChanged[32];

	DECLARE_NO_COPY_CLASS(CDSPRegTable);

public:
	CDSPRegTable()
	{
		memset(m_CachedRegs, 0, sizeof(m_CachedRegs));
		memset(m_CachedRegHasChanged, 0, sizeof(m_CachedRegHasChanged));
	}

	int GetNumberCols(void) {return 2;}
	int GetNumberRows(void) {return 32;}
	bool IsEmptyCell(int row, int col) {return false;}
	wxString GetValue(int row, int col);
	void SetValue(int row, int col, const wxString &);
	wxGridCellAttr *GetAttr(int, int, wxGridCellAttr::wxAttrKind);
	void UpdateCachedRegs();
};

class DSPRegisterView : public wxGrid
{
public:
	DSPRegisterView(wxWindow* parent, wxWindowID id);
	void Update();
};

#endif //__DSPREGISTERVIEW_h__
