// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sstream>
#include <string>
#include <vector>
#include <SFML/Network/Http.hpp>
#include <wx/button.h>
#include <wx/chartype.h>
#include <wx/checklst.h>
#include <wx/defs.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>
#include <wx/window.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/Core.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Cheats/GeckoCodeDiag.h"

namespace Gecko
{

static const wxString wxstr_name(wxTRANSLATE("Name: ")),
	wxstr_notes(wxTRANSLATE("Notes: ")),
	wxstr_creator(wxTRANSLATE("Creator: "));

CodeConfigPanel::CodeConfigPanel(wxWindow* const parent)
	: wxPanel(parent, -1)
{
	m_listbox_gcodes = new wxCheckListBox(this, -1);
	m_listbox_gcodes->Bind(wxEVT_LISTBOX, &CodeConfigPanel::UpdateInfoBox, this);
	m_listbox_gcodes->Bind(wxEVT_CHECKLISTBOX, &CodeConfigPanel::ToggleCode, this);

	m_infobox.label_name = new wxStaticText(this, -1, wxGetTranslation(wxstr_name));
	m_infobox.label_creator = new wxStaticText(this, -1, wxGetTranslation(wxstr_creator));
	m_infobox.label_notes = new wxStaticText(this, -1, wxGetTranslation(wxstr_notes));
	m_infobox.textctrl_notes = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(64, -1), wxTE_MULTILINE | wxTE_READONLY);
	m_infobox.listbox_codes = new wxListBox(this, -1, wxDefaultPosition, wxSize(-1, 64));

	// TODO: buttons to add/edit codes

	// sizers
	wxBoxSizer* const sizer_infobox = new wxBoxSizer(wxVERTICAL);
	sizer_infobox->Add(m_infobox.label_name, 0, wxBOTTOM, 5);
	sizer_infobox->Add(m_infobox.label_creator, 0,  wxBOTTOM, 5);
	sizer_infobox->Add(m_infobox.label_notes, 0, wxBOTTOM, 5);
	sizer_infobox->Add(m_infobox.textctrl_notes, 0, wxBOTTOM | wxEXPAND, 5);
	sizer_infobox->Add(m_infobox.listbox_codes, 1, wxEXPAND, 5);

	// button sizer
	wxBoxSizer* const sizer_buttons = new wxBoxSizer(wxHORIZONTAL);
	btn_download = new wxButton(this, -1, _("Download Codes (WiiRD Database)"), wxDefaultPosition, wxSize(128, -1));
	btn_download->Enable(false);
	btn_download->Bind(wxEVT_BUTTON, &CodeConfigPanel::DownloadCodes, this);
	sizer_buttons->AddStretchSpacer(1);
	sizer_buttons->Add(btn_download, 1, wxEXPAND);

	// horizontal sizer
	wxBoxSizer* const sizer_vert = new wxBoxSizer(wxVERTICAL);
	sizer_vert->Add(sizer_infobox, 1, wxEXPAND);
	sizer_vert->Add(sizer_buttons, 0, wxEXPAND | wxTOP, 5);

	wxBoxSizer* const sizer_main = new wxBoxSizer(wxVERTICAL);
	sizer_main->Add(m_listbox_gcodes, 1, wxALL | wxEXPAND, 5);
	sizer_main->Add(sizer_vert, 0, wxALL | wxEXPAND, 5);

	SetSizerAndFit(sizer_main);
}

void CodeConfigPanel::UpdateCodeList(bool checkRunning)
{
	// disable the button if it doesn't have an effect
	btn_download->Enable((!checkRunning || Core::IsRunning()) && !m_gameid.empty());

	m_listbox_gcodes->Clear();
	// add the codes to the listbox
	for (const GeckoCode& code : m_gcodes)
	{
		m_listbox_gcodes->Append(StrToWxStr(code.name));
		if (code.enabled)
		{
			m_listbox_gcodes->Check(m_listbox_gcodes->GetCount()-1, true);
		}
	}

	wxCommandEvent evt;
	UpdateInfoBox(evt);
}

void CodeConfigPanel::LoadCodes(const IniFile& globalIni, const IniFile& localIni, const std::string& gameid, bool checkRunning)
{
	m_gameid = gameid;

	m_gcodes.clear();
	if (!checkRunning || Core::IsRunning())
		Gecko::LoadCodes(globalIni, localIni, m_gcodes);

	UpdateCodeList(checkRunning);
}

void CodeConfigPanel::ToggleCode(wxCommandEvent& evt)
{
	const int sel = evt.GetInt(); // this right?
	if (sel > -1)
		m_gcodes[sel].enabled = m_listbox_gcodes->IsChecked(sel);
}

void CodeConfigPanel::UpdateInfoBox(wxCommandEvent&)
{
	m_infobox.listbox_codes->Clear();
	const int sel = m_listbox_gcodes->GetSelection();

	if (sel > -1)
	{
		m_infobox.label_name->SetLabel(wxGetTranslation(wxstr_name) + StrToWxStr(m_gcodes[sel].name));

		// notes textctrl
		m_infobox.textctrl_notes->Clear();
		for (const std::string& note : m_gcodes[sel].notes)
		{
			m_infobox.textctrl_notes->AppendText(StrToWxStr(note));
		}
		m_infobox.textctrl_notes->ScrollLines(-99); // silly

		m_infobox.label_creator->SetLabel(wxGetTranslation(wxstr_creator) + StrToWxStr(m_gcodes[sel].creator));

		// add codes to info listbox
		for (const GeckoCode::Code& code : m_gcodes[sel].codes)
		{
			m_infobox.listbox_codes->Append(wxString::Format("%08X %08X", code.address, code.data));
		}
	}
	else
	{
		m_infobox.label_name->SetLabel(wxGetTranslation(wxstr_name));
		m_infobox.textctrl_notes->Clear();
		m_infobox.label_creator->SetLabel(wxGetTranslation(wxstr_creator));
	}
}

void CodeConfigPanel::DownloadCodes(wxCommandEvent&)
{
	if (m_gameid.empty())
		return;

	std::string gameid = m_gameid;


	switch (m_gameid[0])
	{
	case 'R':
	case 'S':
	case 'G':
		break;
	default:
	// All channels (WiiWare, VirtualConsole, etc) are identified by their first four characters
		gameid = m_gameid.substr(0, 4);
		break;
	}

	sf::Http::Request req;
	req.SetURI("/txt.php?txt=" + gameid);

	sf::Http http;
	http.SetHost("geckocodes.org");

	const sf::Http::Response resp = http.SendRequest(req, 5.0f);

	if (sf::Http::Response::Ok == resp.GetStatus())
	{
		// temp vector containing parsed codes
		std::vector<GeckoCode> gcodes;

		// parse the codes
		std::istringstream ss(resp.GetBody());

		std::string line;

		// seek past the header, get to the first code
		std::getline(ss, line);
		std::getline(ss, line);
		std::getline(ss, line);

		int read_state = 0;
		GeckoCode gcode;

		while ((std::getline(ss, line).good()))
		{
			// empty line
			if (0 == line.size() || line == "\r" || line == "\n") // \r\n checks might not be needed
			{
				// add the code
				if (gcode.codes.size())
					gcodes.push_back(gcode);
				gcode = GeckoCode();
				read_state = 0;
				continue;
			}

			switch (read_state)
			{
				// read new code
			case 0 :
			{
				std::istringstream ssline(line);
				// stop at [ character (beginning of contributor name)
				std::getline(ssline, gcode.name, '[');
				gcode.name = StripSpaces(gcode.name);
				gcode.user_defined = true;
				// read the code creator name
				std::getline(ssline, gcode.creator, ']');
				read_state = 1;
			}
				break;

				// read code lines
			case 1 :
			{
				std::istringstream ssline(line);
				std::string addr, data;
				ssline >> addr >> data;
				ssline.seekg(0);

				// check if this line a code, silly, but the dumb txt file comment lines can start with valid hex chars :/
				if (8 == addr.length() && 8 == data.length())
				{
					GeckoCode::Code new_code;
					new_code.original_line = line;
					ssline >> std::hex >> new_code.address >> new_code.data;
					gcode.codes.push_back(new_code);
				}
				else
				{
					gcode.notes.push_back(line);
					read_state = 2; // start reading comments
				}

			}
				break;

				// read comment lines
			case 2 :
				// append comment line
				gcode.notes.push_back(line);
				break;

			}
		}

		// add the last code
		if (gcode.codes.size())
			gcodes.push_back(gcode);

		if (gcodes.size())
		{
			unsigned long added_count = 0;

			// append the codes to the code list
			for (const GeckoCode& code : gcodes)
			{
				// only add codes which do not already exist
				std::vector<GeckoCode>::const_iterator
					existing_gcodes_iter = m_gcodes.begin(),
					existing_gcodes_end = m_gcodes.end();
				for (;; ++existing_gcodes_iter)
				{
					if (existing_gcodes_end == existing_gcodes_iter)
					{
						m_gcodes.push_back(code);
						++added_count;
						break;
					}

					// code exists
					if (existing_gcodes_iter->Compare(code))
						break;
				}
			}

			wxMessageBox(wxString::Format(_("Downloaded %lu codes. (added %lu)"),
				(unsigned long)gcodes.size(), added_count));

			// refresh the list
			UpdateCodeList();
		}
		else
		{
			wxMessageBox(_("File contained no codes."));
		}
	}
	else
	{
		WxUtils::ShowErrorDialog(_("Failed to download codes."));
	}
}

}

