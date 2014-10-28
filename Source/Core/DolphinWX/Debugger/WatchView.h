// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <wx/defs.h>
#include <wx/grid.h>
#include <wx/string.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"

class wxWindow;

class CWatchTable : public wxGridTableBase
{
	enum
	{
		MAX_SPECIALS = 256,
	};

public:
	CWatchTable()
	{
	}

	int GetNumberCols() override { return 5; }
	int GetNumberRows() override { return MAX_SPECIALS; }
	wxString GetValue(int row, int col) override;
	void SetValue(int row, int col, const wxString&) override;
	wxGridCellAttr* GetAttr(int, int, wxGridCellAttr::wxAttrKind) override;
	void UpdateWatch();

private:
	std::array<u32, MAX_SPECIALS> m_CachedWatch;
	std::array<bool, MAX_SPECIALS> m_CachedWatchHasChanged;

	DECLARE_NO_COPY_CLASS(CWatchTable);
};

class CWatchView : public wxGrid
{
public:
	CWatchView(wxWindow* parent, wxWindowID id);
	void Update() override;
	void OnMouseDownR(wxGridEvent& event);
	void OnPopupMenu(wxCommandEvent& event);

private:
	u32 m_selectedAddress = 0;
	u32 m_selectedRow = 0;
};
