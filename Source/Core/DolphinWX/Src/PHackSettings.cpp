// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "PHackSettings.h"
#include "ConfigManager.h"
#include "WxUtils.h"

extern PHackData PHack_Data;

BEGIN_EVENT_TABLE(CPHackSettings, wxDialog)
	EVT_CHOICE(ID_PHACK_CHOICE, CPHackSettings::SetRefresh)
	EVT_BUTTON(wxID_OK, CPHackSettings::SavePHackData)
END_EVENT_TABLE()

CPHackSettings::CPHackSettings(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();
	std::string _iniFilename;
	_iniFilename = File::GetUserPath(D_GAMECONFIG_IDX) + "PH_PRESETS.ini";
	PHPresetsIni.Load(_iniFilename.c_str());
	//PHPresetsIni.SortSections();
	//PHPresetsIni.Save(_iniFilename.c_str());

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
	PHackExP = new wxCheckBox(this, ID_PHACK_EXP, _("Extra Parameter"));
	PHackExP->SetToolTip(_("Extra Parameter useful in '\'Metroid: Other M'\' only."));

	wxStaticBoxSizer *sbPHackSettings = new wxStaticBoxSizer(wxVERTICAL, this, _("Parameters"));
	wxFlexGridSizer *szrPHackSettings = new wxFlexGridSizer(3, 5, 5);
	sbPHackSettings->Add(szrPHackSettings, 0, wxEXPAND|wxLEFT|wxTOP, 5);
	szrPHackSettings->Add(PHackZNearText, 0, wxALIGN_CENTER_VERTICAL);
	szrPHackSettings->Add(PHackZNear, 1, wxEXPAND);
	szrPHackSettings->Add(PHackSZNear, 0, wxEXPAND|wxLEFT, 5);
	szrPHackSettings->Add(PHackZFarText, 0, wxALIGN_CENTER_VERTICAL);
	szrPHackSettings->Add(PHackZFar, 1, wxEXPAND);
	szrPHackSettings->Add(PHackSZFar, 0, wxEXPAND|wxLEFT, 5);
	szrPHackSettings->Add(PHackExP, 0, wxEXPAND|wxTOP|wxBOTTOM, 5);

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
	char sIndex[15];

	PHackChoice->Clear();
	PHackChoice->Append(_("[Custom]"));
	for (int i=0 ;  ; i++)
	{
		sprintf(sIndex,"%d",i);
		if (!PHPresetsIni.Exists(sIndex, "Title"))
			break;
		PHPresetsIni.Get(sIndex, "Title", &sTemp);
		if (sTemp.empty())
			sTemp = wxString(_("(UNKNOWN)")).char_str();
		if (i == 0)
			PHackChoice->Append(StrToWxStr("-------------"));
		PHackChoice->Append(StrToWxStr(sTemp));
	}
	PHackChoice->Select(0);

	PHackSZNear->Set3StateValue((wxCheckBoxState)PHack_Data.PHackSZNear);
	PHackSZFar->Set3StateValue((wxCheckBoxState)PHack_Data.PHackSZFar);
	PHackExP->Set3StateValue((wxCheckBoxState)PHack_Data.PHackExP);

	PHackZNear->SetValue(StrToWxStr(PHack_Data.PHZNear));
	PHackZFar->SetValue(StrToWxStr(PHack_Data.PHZFar));
}

void CPHackSettings::SetRefresh(wxCommandEvent& event)
{
	bool bTemp;
	std::string sTemp;
	char sIndex[15];
	int index = event.GetSelection();
	if (index > 1)
	{
		index -= 2;
		sprintf(sIndex,"%d", index);

		PHPresetsIni.Get(sIndex, "PH_SZNear", &bTemp);
		PHackSZNear->Set3StateValue((wxCheckBoxState)bTemp);
		PHPresetsIni.Get(sIndex, "PH_SZFar", &bTemp);
		PHackSZFar->Set3StateValue((wxCheckBoxState)bTemp);
		PHPresetsIni.Get(sIndex, "PH_ExtraParam", &bTemp);
		PHackExP->Set3StateValue((wxCheckBoxState)bTemp);
		PHPresetsIni.Get(sIndex, "PH_ZNear", &sTemp);
		PHackZNear->SetValue(StrToWxStr(sTemp));
		PHPresetsIni.Get(sIndex, "PH_ZFar", &sTemp);
		PHackZFar->SetValue(StrToWxStr(sTemp));
	}
}

void CPHackSettings::SavePHackData(wxCommandEvent& event)
{
	PHack_Data.PHackSZNear = PHackSZNear->GetValue();
	PHack_Data.PHackSZFar = PHackSZFar->GetValue();
	PHack_Data.PHackExP = PHackExP->GetValue();

	PHack_Data.PHZNear = PHackZNear->GetValue().char_str();
	PHack_Data.PHZFar = PHackZFar->GetValue().char_str();

	AcceptAndClose();
	event.Skip();
}
