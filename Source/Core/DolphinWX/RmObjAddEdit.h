// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Core/RmObjEngine.h"

class wxButton;
class wxRadioBox;
class wxSpinButton;
class wxSpinEvent;
class wxStaticBoxSizer;
class wxTextCtrl;
class wxWindow;

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
	wxSpinButton* EntrySelection;
	wxButton* EntryAdd;
	wxButton* EntryRemove;
	wxStaticBoxSizer* sbEntry;

	void CreateGUIControls(int selection);
	void SaveRmObjData(wxCommandEvent& event);
	bool UpdateTempEntryData(std::vector<RmObjEngine::RmObjEntry>::iterator iterEntry);

	int selection, currentItem;
	std::vector<RmObjEngine::RmObjEntry> tempEntries;
	std::vector<RmObjEngine::RmObjEntry>::iterator itCurEntry;
};
