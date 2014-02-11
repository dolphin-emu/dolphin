// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/mstream.h>
#include <wx/statbmp.h>

class AboutDolphin : public wxDialog
{
	public:
		AboutDolphin(wxWindow *parent,
			wxWindowID id = wxID_ANY,
			const wxString &title = _("About Dolphin"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE);
};
