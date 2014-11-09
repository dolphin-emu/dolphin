// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/bitmap.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/gdicmn.h>
#include <wx/image.h>
#include <wx/mstream.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Common/Common.h"
#include "DolphinWX/AboutDolphin.h"
#include "DolphinWX/resources/dolphin_logo.cpp"

AboutDolphin::AboutDolphin(wxWindow *parent, wxWindowID id,
		const wxString &title, const wxPoint &position,
		const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	wxMemoryInputStream istream(dolphin_logo_png, sizeof dolphin_logo_png);
	wxImage iDolphinLogo(istream, wxBITMAP_TYPE_PNG);
	wxStaticBitmap* const sbDolphinLogo = new wxStaticBitmap(this, wxID_ANY,
			wxBitmap(iDolphinLogo));

	const wxString Text = wxString::Format(_("Dolphin %s\n"
				"Copyright (c) 2003-2014+ Dolphin Team\n"
				"\n"
				"Branch: %s\n"
				"Revision: %s\n"
				"Compiled: %s @ %s\n"
				"\n"
				"Dolphin is a GameCube/Wii emulator, which was\n"
				"originally written by F|RES and ector.\n"
				"Today Dolphin is an open source project with many\n"
				"contributors, too many to list.\n"
				"If interested, just go check out the project page at\n"
				"https://github.com/dolphin-emu/dolphin/ .\n"
				"\n"
				"Special thanks to Bushing, Costis, CrowTRobo,\n"
				"Marcan, Segher, Titanik, or9 and Hotquik for their\n"
				"reverse engineering and docs/demos.\n"
				"\n"
				"Big thanks to Gilles Mouchard whose Microlib PPC\n"
				"emulator gave our development a kickstart.\n"
				"\n"
				"Thanks to Frank Wille for his PowerPC disassembler,\n"
				"which or9 and we modified to include Gekko specifics.\n"
				"\n"
				"Thanks to hcs/destop for their GC ADPCM decoder.\n"
				"\n"
				"We are not affiliated with Nintendo in any way.\n"
				"GameCube and Wii are trademarks of Nintendo.\n"
				"The emulator should not be used to play games\n"
				"you do not legally own."),
		scm_desc_str, scm_branch_str, scm_rev_git_str, __DATE__, __TIME__);

	wxStaticText* const Message = new wxStaticText(this, wxID_ANY, Text);
	Message->Wrap(GetSize().GetWidth());

	wxBoxSizer* const sInfo = new wxBoxSizer(wxVERTICAL);
	sInfo->Add(Message, 1, wxEXPAND | wxALL, 5);

	wxBoxSizer* const sMainHor = new wxBoxSizer(wxHORIZONTAL);
	sMainHor->Add(sbDolphinLogo, 0, wxEXPAND | wxALL, 5);
	sMainHor->Add(sInfo);

	wxBoxSizer* const sMain = new wxBoxSizer(wxVERTICAL);
	sMain->Add(sMainHor, 1, wxEXPAND);
	sMain->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	SetSizerAndFit(sMain);
	Center();
	SetFocus();
}
