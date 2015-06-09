// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <wx/dialog.h>

#include "Core/PatchEngine.h"

class wxButton;
class wxRadioBox;
class wxSpinButton;
class wxSpinEvent;
class wxStaticBoxSizer;
class wxTextCtrl;

class CPatchAddEdit : public wxDialog
{
public:
	CPatchAddEdit(int _selection, std::vector<PatchEngine::Patch>* _onFrame,
		wxWindow* parent,
		wxWindowID id = wxID_ANY,
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
	std::vector<PatchEngine::Patch>* onFrame;

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
