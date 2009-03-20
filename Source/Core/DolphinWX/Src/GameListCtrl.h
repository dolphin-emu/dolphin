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

#ifndef __GAMELIST_CTRL_H_
#define __GAMELIST_CTRL_H_

#include <vector>

#include <wx/listctrl.h>

#include "ISOFile.h"

class CGameListCtrl : public wxListCtrl
{
public:

	CGameListCtrl(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style);
	~CGameListCtrl();

	void Update();
	void BrowseForDirectory();
	const GameListItem *GetSelectedISO();
	const GameListItem *GetISO(int index) const;

	enum
	{
		COLUMN_BANNER = 0,
		COLUMN_TITLE,
		COLUMN_COMPANY,
		COLUMN_NOTES,
		COLUMN_COUNTRY,
		COLUMN_SIZE,
		COLUMN_EMULATION_STATE,
		NUMBER_OF_COLUMN
	};

private:

	std::vector<int> m_FlagImageIndex;
	std::vector<GameListItem> m_ISOFiles;

	int last_column;
	int last_sort;

	void InitBitmaps();
	void InsertItemInReportView(long _Index);
	void SetBackgroundColor();
	void ScanForISOs();

	DECLARE_EVENT_TABLE()

	// events
	void OnRightClick(wxMouseEvent& event);
	void OnColumnClick(wxListEvent& event);
	void OnColBeginDrag(wxListEvent& event);
	void OnSize(wxSizeEvent& event);
	void OnProperties(wxCommandEvent& event);
	void OnOpenContainingFolder(wxCommandEvent& event);
	void OnSetDefaultGCM(wxCommandEvent& event);
	void OnDeleteGCM(wxCommandEvent& event);
	void OnCompressGCM(wxCommandEvent& event);
	void OnMultiCompressGCM(wxCommandEvent& event);
	void OnMultiDecompressGCM(wxCommandEvent& event);

	void CompressSelection(bool _compress);
	void AutomaticColumnWidth();
	void UnselectAll();

	static size_t m_currentItem;
	static std::string m_currentFilename;
	static size_t m_numberItem;
	static void CompressCB(const char* text, float percent, void* arg);
	static void MultiCompressCB(const char* text, float percent, void* arg);

	// hyperiris: put it here will be nice, if we moce to wx unicode, it simple to fix
	bool CopySJISToString(wxString& _rDestination, const char* _src);
};


#endif

