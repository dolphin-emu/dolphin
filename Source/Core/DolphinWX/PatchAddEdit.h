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

#include "Core/PatchEngine.h"

class wxButton;
class wxRadioBox;
class wxSpinButton;
class wxSpinEvent;
class wxStaticBoxSizer;
class wxTextCtrl;
class wxWindow;

class CPatchAddEdit : public wxDialog
{
public:
	CPatchAddEdit(int _selection, wxWindow* parent,
		wxWindowID id = 1,
		const wxString& title = _("Edit Patch"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxDEFAULT_DIALOG_STYLE);
	virtual ~CPatchAddEdit();

private:
	wxTextCtrl* EditPatchName;
	wxTextCtrl* EditPatchOffset;
	wxRadioBox* EditPatchType;
	wxTextCtrl* EditPatchValue;
	wxSpinButton* EntrySelection;
	wxButton* EntryAdd;
	wxButton* EntryRemove;
	wxStaticBoxSizer* sbEntry;

	void CreateGUIControls(int selection);
	void ChangeEntry(wxSpinEvent& event);
	void SavePatchData(wxCommandEvent& event);
	void AddEntry(wxCommandEvent& event);
	void RemoveEntry(wxCommandEvent& event);
	void UpdateEntryCtrls(PatchEngine::PatchEntry pE);
	bool UpdateTempEntryData(std::vector<PatchEngine::PatchEntry>::iterator iterEntry);

	int selection, currentItem;
	std::vector<PatchEngine::PatchEntry> tempEntries;
	std::vector<PatchEngine::PatchEntry>::iterator itCurEntry;
};
