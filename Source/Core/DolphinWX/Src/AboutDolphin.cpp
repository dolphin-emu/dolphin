// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "AboutDolphin.h"
#include "WxUtils.h"
#include "../resources/dolphin_logo.cpp"
#include "scmrev.h"

AboutDolphin::AboutDolphin(wxWindow *parent, wxWindowID id,
		const wxString &title, const wxPoint &position,
		const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	wxMemoryInputStream istream(dolphin_logo_png, sizeof dolphin_logo_png);
	wxImage iDolphinLogo(istream, wxBITMAP_TYPE_PNG);
	wxStaticBitmap* const sbDolphinLogo = new wxStaticBitmap(this, wxID_ANY,
			wxBitmap(iDolphinLogo));

	std::string Text = "Dolphin " SCM_DESC_STR "\n"
		"Copyright (c) 2003-2013+ Dolphin Team\n"
		"\n"
		"Branch: " SCM_BRANCH_STR "\n"
		"Revision: " SCM_REV_STR "\n"
		"Compiled: " __DATE__ " @ " __TIME__ "\n"
		"\n"
		"Dolphin is a Gamecube/Wii emulator, which was\n"
		"originally written by F|RES and ector.\n"
		"Today Dolphin is an open source project with many\n"
		"contributors, too many to list.\n"
		"If interested, just go check out the project page at\n"
		"http://code.google.com/p/dolphin-emu/ .\n"
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
		"Gamecube and Wii are trademarks of Nintendo.\n"
		"The emulator is for educational purposes only\n"
		"and should not be used to play games you do\n"
		"not legally own.";
	wxStaticText* const Message = new wxStaticText(this, wxID_ANY,
			StrToWxStr(Text));
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
