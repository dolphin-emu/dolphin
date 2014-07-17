// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdio>
#include <string>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "DolphinWX/ISOProperties.h"
#include "DolphinWX/PHackSettings.h"
#include "DolphinWX/WxUtils.h"

class wxWindow;

BEGIN_EVENT_TABLE(CPHackSettings, wxDialog)
	EVT_CHOICE(ID_PHACK_CHOICE, CPHackSettings::SetRefresh)
	EVT_BUTTON(wxID_OK, CPHackSettings::SavePHackData)
END_EVENT_TABLE()

CPHackSettings::CPHackSettings(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();
	std::string _iniFilename = File::GetSysDirectory() + GAMESETTINGS_DIR DIR_SEP "PH_PRESETS.ini";
	PHPresetsIni.Load(_iniFilename);
	PHPresetsIni.SortSections();

	LoadPHackData();
}

CPHackSettings::~CPHackSettings()
{
}

void CPHackSettings::CreateGUIControls()
{
	wxStaticText *PHackChoiceText = new wxStaticText(this, wxID_ANY, _("Presets: "));
	PHackChoice = new wxChoice(this, ID_PHACK_CHOICE);
	PHackChoice->SetToolTip(_("Load preset values from hack patterns available."));
	wxStaticText *PHackZNearText = new wxStaticText(this, wxID_ANY, _("zNear Correction: "));
	PHackZNear = new wxTextCtrl(this, ID_PHACK_ZNEAR);
	PHackZNear->SetToolTip(_("Adds the specified value to zNear Parameter.\nTwo ways to express the floating point values.\nExample: entering '\'200'\' or '\'0.0002'\' directly, it produces equal effects, the acquired value will be '\'0.0002'\'.\nValues: (0->+/-Integer) or (0->+/-FP[6 digits of precision])\n\nNOTE: Check LogWindow/Console for the acquired values."));
	PHackSZNear = new wxCheckBox(this, ID_PHACK_SZNEAR, _("(-)+zNear"));
	PHackSZNear->SetToolTip(_("Changes sign to zNear Parameter (after correction)"));
	wxStaticText *PHackZFarText = new wxStaticText(this, wxID_ANY, _("zFar Correction: "));
	PHackZFar = new wxTextCtrl(this, ID_PHACK_ZFAR);
	PHackZFar->SetToolTip(_("Adds the specified value to zFar Parameter.\nTwo ways to express the floating point values.\nExample: entering '\'200'\' or '\'0.0002'\' directly, it produces equal effects, the acquired value will be '\'0.0002'\'.\nValues: (0->+/-Integer) or (0->+/-FP[6 digits of precision])\n\nNOTE: Check LogWindow/Console for the acquired values."));
	PHackSZFar = new wxCheckBox(this, ID_PHACK_SZFAR, _("(-)+zFar"));
	PHackSZFar->SetToolTip(_("Changes sign to zFar Parameter (after correction)"));

	wxStaticBoxSizer *sbPHackSettings = new wxStaticBoxSizer(wxVERTICAL, this, _("Parameters"));
	wxFlexGridSizer *szrPHackSettings = new wxFlexGridSizer(3, 5, 5);
	sbPHackSettings->Add(szrPHackSettings, 0, wxEXPAND|wxLEFT|wxTOP, 5);
	szrPHackSettings->Add(PHackZNearText, 0, wxALIGN_CENTER_VERTICAL);
	szrPHackSettings->Add(PHackZNear, 1, wxEXPAND);
	szrPHackSettings->Add(PHackSZNear, 0, wxEXPAND|wxLEFT, 5);
	szrPHackSettings->Add(PHackZFarText, 0, wxALIGN_CENTER_VERTICAL);
	szrPHackSettings->Add(PHackZFar, 1, wxEXPAND);
	szrPHackSettings->Add(PHackSZFar, 0, wxEXPAND|wxLEFT, 5);

	wxBoxSizer* sPHack = new wxBoxSizer(wxVERTICAL);
	sPHack->Add(PHackChoiceText, 0, wxEXPAND|wxLEFT|wxRIGHT|wxTOP, 5);
	sPHack->Add(PHackChoice, 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5);
	sPHack->Add(sbPHackSettings, 0, wxEXPAND|wxALL, 5);
	sPHack->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5);

	SetSizerAndFit(sPHack);
	SetFocus();
}

void CPHackSettings::LoadPHackData()
{
	std::string sTemp;
	std::string sIndex;

	PHackChoice->Clear();
	PHackChoice->Append(_("[Custom]"));
	for (int i = 0; ; i++)
	{
		sIndex = std::to_string(i);

		if (!PHPresetsIni.Exists(sIndex, "Title"))
			break;

		PHPresetsIni.GetOrCreateSection(sIndex)->Get("Title", &sTemp);

		if (sTemp.empty())
			sTemp = WxStrToStr(_("(UNKNOWN)"));

		if (i == 0)
			PHackChoice->Append(StrToWxStr("-------------"));

		PHackChoice->Append(StrToWxStr(sTemp));
	}
	PHackChoice->Select(0);

	PHackSZNear->Set3StateValue((wxCheckBoxState)PHack_Data.PHackSZNear);
	PHackSZFar->Set3StateValue((wxCheckBoxState)PHack_Data.PHackSZFar);

	PHackZNear->SetValue(StrToWxStr(PHack_Data.PHZNear));
	PHackZFar->SetValue(StrToWxStr(PHack_Data.PHZFar));
}

void CPHackSettings::SetRefresh(wxCommandEvent& event)
{
	bool bTemp;
	std::string sTemp;
	std::string sIndex;

	int index = event.GetSelection();
	if (index > 1)
	{
		index -= 2;
		sIndex = std::to_string(index);

		IniFile::Section* proj_hack = PHPresetsIni.GetOrCreateSection(sIndex);
		proj_hack->Get("PH_SZNear", &bTemp);
		PHackSZNear->Set3StateValue((wxCheckBoxState)bTemp);
		proj_hack->Get("PH_SZFar", &bTemp);
		PHackSZFar->Set3StateValue((wxCheckBoxState)bTemp);
		proj_hack->Get("PH_ZNear", &sTemp);
		PHackZNear->SetValue(StrToWxStr(sTemp));
		proj_hack->Get("PH_ZFar", &sTemp);
		PHackZFar->SetValue(StrToWxStr(sTemp));
	}
}

void CPHackSettings::SavePHackData(wxCommandEvent& event)
{
	PHack_Data.PHackSZNear = PHackSZNear->GetValue();
	PHack_Data.PHackSZFar = PHackSZFar->GetValue();

	PHack_Data.PHZNear = PHackZNear->GetValue().char_str();
	PHack_Data.PHZFar = PHackZFar->GetValue().char_str();

	AcceptAndClose();
	event.Skip();
}
