// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

class CRmObjAddEdit : public wxDialog
{
public:
	CRmObjAddEdit(int _selection, wxWindow* parent,
		wxWindowID id = 1,
		const wxString& title = _("Edit Object Removal Code"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxDEFAULT_DIALOG_STYLE);
	virtual ~CRmObjAddEdit();

private:
	wxTextCtrl* EditRmObjName;
	wxTextCtrl* EditRmObjOffset;
	wxRadioBox* EditRmObjType;
	wxTextCtrl* EditRmObjValue;
	wxStaticBoxSizer* sbEntry;

	void CreateGUIControls(int selection);
	void SaveRmObjData(wxCommandEvent& event);
	void ButtonUporDown(wxCommandEvent& event);
	bool UpdateTempEntryData(std::vector<RmObjEngine::RmObjEntry>::iterator iterEntry);
	bool ParseValue(unsigned long long value, RmObjEngine::RmObjType tempType);

	enum
	{
		ID_BUTTON_UP,
		ID_BUTTON_DOWN
	};

	int selection;
	std::vector<RmObjEngine::RmObjEntry> tempEntries;
	std::vector<RmObjEngine::RmObjEntry>::iterator itCurEntry;
};
