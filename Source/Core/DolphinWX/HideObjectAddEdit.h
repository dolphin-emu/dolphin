// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

class CHideObjectAddEdit : public wxDialog
{
public:
	CHideObjectAddEdit(int _selection, wxWindow* parent,
		wxWindowID id = wxID_ANY,
		const wxString& title = _("Edit Object Removal Code"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxDEFAULT_DIALOG_STYLE);
	virtual ~CHideObjectAddEdit();

private:
	wxTextCtrl* EditHideObjectName;
	wxTextCtrl* EditHideObjectOffset;
	wxRadioBox* EditHideObjectType;
	wxTextCtrl* EditHideObjectValue;
	wxStaticBoxSizer* sbEntry;

	void CreateGUIControls(int selection);
	void SaveHideObjectData(wxCommandEvent& event);
	void ButtonUporDown(wxCommandEvent& event);
	bool UpdateTempEntryData(std::vector<HideObjectEngine::HideObjectEntry>::iterator iterEntry);
	bool ParseValue(unsigned long long value, HideObjectEngine::HideObjectType tempType);

	enum
	{
		ID_BUTTON_UP,
		ID_BUTTON_DOWN
	};

	int selection;
	std::vector<HideObjectEngine::HideObjectEntry> tempEntries;
	std::vector<HideObjectEngine::HideObjectEntry>::iterator itCurEntry;
};
