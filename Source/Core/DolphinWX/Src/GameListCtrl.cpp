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

#include <wx/imaglist.h>
#include <algorithm>

#include "FileSearch.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "BootManager.h"
#include "Config.h"
#include "GameListCtrl.h"

#if USE_XPM_BITMAPS
    #include "../resources/Flag_Europe.xpm"
    #include "../resources/Flag_France.xpm"
    #include "../resources/Flag_Japan.xpm"
    #include "../resources/Flag_USA.xpm"
#endif // USE_XPM_BITMAPS

BEGIN_EVENT_TABLE(CGameListCtrl, wxListCtrl)

EVT_SIZE(CGameListCtrl::OnSize)
EVT_RIGHT_DOWN(CGameListCtrl::OnRightClick)
EVT_LIST_COL_BEGIN_DRAG(LIST_CTRL, CGameListCtrl::OnColBeginDrag)
EVT_LIST_ITEM_SELECTED(LIST_CTRL, CGameListCtrl::OnSelected)
EVT_LIST_ITEM_ACTIVATED(LIST_CTRL, CGameListCtrl::OnActivated)
EVT_LIST_COL_END_DRAG(LIST_CTRL, CGameListCtrl::OnColEndDrag)
EVT_MENU(IDM_EDITPATCHFILE, CGameListCtrl::OnEditPatchFile)
EVT_MENU(IDM_OPENCONTAININGFOLDER, CGameListCtrl::OnOpenContainingFolder)
EVT_MENU(IDM_SETDEFAULTGCM, CGameListCtrl::OnSetDefaultGCM)
END_EVENT_TABLE()

CGameListCtrl::CGameListCtrl(wxWindow* parent, const wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
	: wxListCtrl(parent, id, pos, size, style)                                                                                                                 // | wxLC_VIRTUAL)
{
	InitBitmaps();
}

void CGameListCtrl::InitBitmaps()
{
	m_imageListSmall = new wxImageList(96, 32);
	SetImageList(m_imageListSmall, wxIMAGE_LIST_SMALL);
	m_FlagImageIndex.resize(DiscIO::IVolume::NUMBER_OF_COUNTRIES);
	wxIcon iconTemp;
	iconTemp.CopyFromBitmap(wxBitmap(Flag_Europe_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_EUROPE] = m_imageListSmall->Add(iconTemp);
	iconTemp.CopyFromBitmap(wxBitmap(Flag_France_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_FRANCE] = m_imageListSmall->Add(iconTemp);
	iconTemp.CopyFromBitmap(wxBitmap(Flag_USA_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_USA] = m_imageListSmall->Add(iconTemp);
	iconTemp.CopyFromBitmap(wxBitmap(Flag_Japan_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_JAP] = m_imageListSmall->Add(iconTemp);
	iconTemp.CopyFromBitmap(wxBitmap(Flag_Europe_xpm));
	m_FlagImageIndex[DiscIO::IVolume::COUNTRY_UNKNOWN] = m_imageListSmall->Add(iconTemp);
}

void CGameListCtrl::BrowseForDirectory()
{
	wxString dirHome;
	wxGetHomeDir(&dirHome);

	// browse
	wxDirDialog dialog(this, _T("Browse directory"), dirHome, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);

	if (dialog.ShowModal() == wxID_OK)
	{
		SConfig::GetInstance().m_ISOFolder.push_back(
			std::string(dialog.GetPath().ToAscii())
			);
		SConfig::GetInstance().SaveSettings();
		Update();
	}
}

void CGameListCtrl::Update()
{
	Hide();

	ScanForISOs();

	ClearAll();

	if (m_ISOFiles.size() != 0)
	{
		// add columns
		InsertColumn(COLUMN_BANNER, _T("Banner"));
		InsertColumn(COLUMN_TITLE, _T("Title"));
		InsertColumn(COLUMN_COMPANY, _T("Company"));
		InsertColumn(COLUMN_NOTES, _T("Notes"));
		InsertColumn(COLUMN_COUNTRY, _T(""));
		InsertColumn(COLUMN_SIZE, _T("Size"));
		InsertColumn(COLUMN_EMULATION_STATE, _T("Emulation"));

		// set initial sizes for columns
		SetColumnWidth(COLUMN_BANNER, 106);
		SetColumnWidth(COLUMN_TITLE, 150);
		SetColumnWidth(COLUMN_COMPANY, 100);
		SetColumnWidth(COLUMN_NOTES, 200);
		SetColumnWidth(COLUMN_COUNTRY, 32);
		SetColumnWidth(COLUMN_EMULATION_STATE, 75);

		// add all items
		for (int i = 0; i < (int)m_ISOFiles.size(); i++)
		{
			InsertItemInReportView(i);
		}

		SetColumnWidth(COLUMN_SIZE, wxLIST_AUTOSIZE);
	}
	else
	{
		InsertColumn(COLUMN_BANNER, _T("No ISOs found"));

		// data
		wxString buf(_T("Dolphin could not find any GC/Wii ISOs. Doubleclick here to browse for files..."));
		long item = InsertItem(0, buf, -1);
		SetItemFont(item, *wxITALIC_FONT);
		SetColumnWidth(item, wxLIST_AUTOSIZE);
	}

	AutomaticColumnWidth();

	Show();
}

#ifdef _WIN32
wxColour blend50(const wxColour& c1, const wxColour& c2)
{
	return(((c1.GetPixel() & 0xFEFEFE) >> 1) + ((c2.GetPixel() & 0xFEFEFE) >> 1) + 0x010101);
}
#endif

wxString NiceSizeFormat(s64 _size)
{
	const char* sizes[] = {"b", "KB", "MB", "GB", "TB", "PB", "EB"};
	int s = 0;
	int frac = 0;
	
	while (_size > (s64)1024)
	{
		s++;
		frac   = (int)_size & 1023;
		_size /= (s64)1024;
	}

	float f = (float)_size + ((float)frac / 1024.0f);

	wxString NiceString;
	char tempstr[32];
	sprintf(tempstr,"%3.1f %s", f, sizes[s]);
	NiceString = wxString::FromAscii(tempstr);
	return(NiceString);
}

void CGameListCtrl::InsertItemInReportView(long _Index)
{
	CISOFile& rISOFile = m_ISOFiles[_Index];

	int ImageIndex = -1;

	if (rISOFile.GetImage().IsOk())
	{
		ImageIndex = m_imageListSmall->Add(rISOFile.GetImage());
	}

	// data
	wxString buf;
	long ItemIndex = InsertItem(_Index, buf, ImageIndex);
#ifdef _WIN32
	wxColour color = (_Index & 1) ? blend50(GetSysColor(COLOR_3DLIGHT), GetSysColor(COLOR_WINDOW)) : GetSysColor(COLOR_WINDOW);
#else
	wxColour color = (_Index & 1) ? 0xFFFFFF : 0xEEEEEE;
#endif
	// background color
	{
		wxListItem item;
		item.SetId(ItemIndex);
		item.SetBackgroundColour(color);
		SetItem(item);
	}

	// title
	{
		wxListItem item;
		item.SetId(ItemIndex);
		item.SetColumn(COLUMN_TITLE);
		//SetItemTextColour(item, (wxColour(0xFF0000)));
		item.SetText(wxString::FromAscii(rISOFile.GetName().c_str()));
		SetItem(item);
	}

	// company
	{
		wxListItem item;
		item.SetId(ItemIndex);
		item.SetColumn(COLUMN_COMPANY);
		//SetItemTextColour(item, (wxColour(0x007030)));
		item.SetText(wxString::FromAscii(rISOFile.GetCompany().c_str()));
		SetItem(item);
	}

	// description
	{
		wxListItem item;
		item.SetId(ItemIndex);
		item.SetColumn(COLUMN_NOTES);
		item.SetText(wxString::FromAscii(rISOFile.GetDescription().c_str()));
		SetItem(item);
	}

	// size
	{
		wxListItem item;
		item.SetId(ItemIndex);
		item.SetColumn(COLUMN_SIZE);
		item.SetText(NiceSizeFormat(rISOFile.GetFileSize()));
		SetItem(item);
	}

#ifndef __WXMSW__
	// country
	{
		// Can't do this in Windows - we use DrawSubItem instead, see below
		wxListItem item;
		item.m_itemId = ItemIndex;
		item.SetColumn(COLUMN_COUNTRY);
		item.SetBackgroundColour(color);
		DiscIO::IVolume::ECountry Country = rISOFile.GetCountry();

		if (size_t(Country) < m_FlagImageIndex.size())
		{
			item.SetImage(m_FlagImageIndex[rISOFile.GetCountry()]);
		}

		SetItem(item);
	}
#endif // __WXMSW__

	// item data
	SetItemData(ItemIndex, _Index);
}

void CGameListCtrl::ScanForISOs()
{
	m_ISOFiles.clear();

	CFileSearch::XStringVector Directories(SConfig::GetInstance().m_ISOFolder);

	CFileSearch::XStringVector Extensions;
	Extensions.push_back("*.iso");
	Extensions.push_back("*.gcm");
	Extensions.push_back("*.gcz");

	CFileSearch FileSearch(Extensions, Directories);
	const CFileSearch::XStringVector& rFilenames = FileSearch.GetFileNames();

	if (rFilenames.size() > 0)
	{
		wxProgressDialog dialog(_T("Scanning for ISOs"),
					_T("Scanning..."),
					rFilenames.size(), // range
					this, // parent
					wxPD_CAN_ABORT |
					wxPD_APP_MODAL |
					// wxPD_AUTO_HIDE | -- try this as well
					wxPD_ELAPSED_TIME |
					wxPD_ESTIMATED_TIME |
					wxPD_REMAINING_TIME |
					wxPD_SMOOTH // - makes indeterminate mode bar on WinXP very small
					);

		dialog.CenterOnParent();

		for (u32 i = 0; i < rFilenames.size(); i++)
		{
			std::string FileName;
			SplitPath(rFilenames[i], NULL, &FileName, NULL);

			wxString msg;
			char tempstring[128];
			sprintf(tempstring,"Scanning %s", FileName.c_str());
			msg = wxString::FromAscii(tempstring);

			bool Cont = dialog.Update(i, msg);

			if (!Cont)
			{
				break;
			}

			CISOFile ISOFile(rFilenames[i]);

			if (ISOFile.IsValid())
			{
				m_ISOFiles.push_back(ISOFile);
			}
		}
	}
	std::sort(m_ISOFiles.begin(), m_ISOFiles.end());
}

void CGameListCtrl::OnColBeginDrag(wxListEvent& event)
{
	if (event.GetColumn() != COLUMN_TITLE && event.GetColumn() != COLUMN_COMPANY
		&& event.GetColumn() != COLUMN_NOTES)
		event.Veto();
}

void CGameListCtrl::OnColEndDrag(wxListEvent& WXUNUSED (event))
{
}

void CGameListCtrl::OnRightClick(wxMouseEvent& event)
{
	// Focus the clicked item.
	int flags;
    long item = HitTest(event.GetPosition(), flags);
	if (item != wxNOT_FOUND) {
		SetItemState(item, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
			               wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
	}
	const CISOFile *selected_iso = GetSelectedISO();
	if (selected_iso) {
		std::string unique_id = selected_iso->GetUniqueID();
		wxMenu popupMenu;
		std::string menu_text = StringFromFormat("Edit &patch file: %s.ini", unique_id.c_str());
		popupMenu.Append(IDM_EDITPATCHFILE, wxString::FromAscii(menu_text.c_str())); //Pretty much everything in wxwidgets is a wxString, try to convert to those first!
		popupMenu.Append(IDM_OPENCONTAININGFOLDER, wxString::FromAscii("Open &containing folder"));
		popupMenu.Append(IDM_SETDEFAULTGCM, wxString::FromAscii("Set as &default ISO"));
		PopupMenu(&popupMenu);
	}
}

void CGameListCtrl::OnActivated(wxListEvent& event)
{
	if (m_ISOFiles.size() == 0)
	{
		BrowseForDirectory();
	}
	else
	{
		size_t Index = event.GetData();
		if (Index < m_ISOFiles.size())
		{
			const CISOFile& rISOFile = m_ISOFiles[Index];
			BootManager::BootCore(rISOFile.GetFileName());
		}
	}
}

const CISOFile * CGameListCtrl::GetSelectedISO() const
{
	int item = GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED); 
	if (item == -1)
		return 0;
	else
		return &m_ISOFiles[GetItemData(item)];
}

void CGameListCtrl::OnOpenContainingFolder(wxCommandEvent& WXUNUSED (event)) {
	const CISOFile *iso = GetSelectedISO();
	if (!iso)
		return;
	std::string path;
	SplitPath(iso->GetFileName(), &path, 0, 0);
	File::Explore(path);
}

void CGameListCtrl::OnSetDefaultGCM(wxCommandEvent& WXUNUSED (event)) {
	const CISOFile *iso = GetSelectedISO();
	if (!iso)
		return;
	SConfig::GetInstance().m_LocalCoreStartupParameter.m_strDefaultGCM = iso->GetFileName();
	SConfig::GetInstance().SaveSettings();
}

void CGameListCtrl::OnEditPatchFile(wxCommandEvent& WXUNUSED (event))
{
	const CISOFile *iso = GetSelectedISO();
	if (!iso)
		return;
	std::string filename = "Patches/" + iso->GetUniqueID() + ".ini";
	if (!File::Exists(filename)) {
		if (AskYesNo("%s.ini does not exist. Do you want to create it?", iso->GetUniqueID().c_str())) {
			FILE *f = fopen(filename.c_str(), "w");
			fprintf(f, "# %s - %s\r\n\r\n", iso->GetUniqueID().c_str(), iso->GetName().c_str());
			fprintf(f, "[OnFrame]\r\n#Add memory patches here.\r\n\r\n");
			fprintf(f, "[ActionReplay]\r\n#Add decrypted action replay cheats here.\r\n");
			fclose(f);
		} else {
			return;
		}
	}
	File::Launch(filename);
}

void CGameListCtrl::OnSelected(wxListEvent& WXUNUSED (event))
{
}

void CGameListCtrl::OnSize(wxSizeEvent& event)
{
	AutomaticColumnWidth();

	event.Skip();
}

bool CGameListCtrl::MSWDrawSubItem(wxPaintDC& rPaintDC, int item, int subitem)
{
	bool Result = false;
#ifdef __WXMSW__
	switch (subitem)
	{
	    case COLUMN_COUNTRY:
	    {
		    size_t Index = GetItemData(item);

		    if (Index < m_ISOFiles.size())
		    {
			    const CISOFile& rISO = m_ISOFiles[Index];
			    wxRect SubItemRect;
			    this->GetSubItemRect(item, subitem, SubItemRect);
			    m_imageListSmall->Draw(m_FlagImageIndex[rISO.GetCountry()], rPaintDC, SubItemRect.GetLeft(), SubItemRect.GetTop());
		    }
	    }
	}
#endif

	return(Result);
}

void CGameListCtrl::AutomaticColumnWidth()
{
	wxRect rc(GetClientRect());

	if (GetColumnCount() == 1)
	{
		SetColumnWidth(0, rc.GetWidth());
	}
	else if (GetColumnCount() > 4)
	{
		int resizable = rc.GetWidth() - (213 + GetColumnWidth(COLUMN_SIZE));

		SetColumnWidth(COLUMN_TITLE, wxMax(0.3*resizable, 100));
		SetColumnWidth(COLUMN_COMPANY, wxMax(0.2*resizable, 100));
		SetColumnWidth(COLUMN_NOTES, wxMax(0.5*resizable, 100));
	}
}
