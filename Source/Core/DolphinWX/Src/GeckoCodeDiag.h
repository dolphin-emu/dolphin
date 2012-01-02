// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

#ifndef __GECKOCODEDIAG_h__
#define __GECKOCODEDIAG_h__

#include "GeckoCode.h"
#include "GeckoCodeConfig.h"

#include "wx/wx.h"

namespace Gecko
{


class CodeConfigPanel : public wxPanel
{
public:
	CodeConfigPanel(wxWindow* const parent);


	void LoadCodes(const IniFile& inifile, const std::string& gameid = "");
	const std::vector<GeckoCode>& GetCodes() const { return m_gcodes; }

protected:
	void UpdateInfoBox(wxCommandEvent&);
	void ToggleCode(wxCommandEvent& evt);
	void DownloadCodes(wxCommandEvent&);
	//void ApplyChanges(wxCommandEvent&);

	void UpdateCodeList();

private:
	std::vector<GeckoCode> m_gcodes;

	std::string m_gameid;

	// wxwidgets stuff
	wxCheckListBox	*m_listbox_gcodes;
	struct
	{
		wxStaticText	*label_name, *label_notes, *label_creator;
		wxTextCtrl		*textctrl_notes;
		wxListBox	*listbox_codes;
	} m_infobox;

};



}

#endif

