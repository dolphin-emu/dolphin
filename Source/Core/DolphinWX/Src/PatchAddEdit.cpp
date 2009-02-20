// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "PatchAddEdit.h"

extern std::vector<PatchEngine::Patch> onFrame;

BEGIN_EVENT_TABLE(CPatchAddEdit, wxDialog)
	EVT_CLOSE(CPatchAddEdit::OnClose)
	EVT_BUTTON(wxID_OK, CPatchAddEdit::SavePatchData)
	EVT_BUTTON(ID_ENTRY_ADD, CPatchAddEdit::AddRemoveEntry)
	EVT_BUTTON(ID_ENTRY_REMOVE, CPatchAddEdit::AddRemoveEntry)
	EVT_SPIN(ID_ENTRY_SELECT, CPatchAddEdit::ChangeEntry)
END_EVENT_TABLE()

CPatchAddEdit::CPatchAddEdit(int _selection, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	selection = _selection;
	CreateGUIControls(selection);
}

CPatchAddEdit::~CPatchAddEdit()
{
}

void CPatchAddEdit::CreateGUIControls(int _selection)
{
	wxString currentName = wxT("<Insert name here>");

	if (_selection == -1)
	{
		tempEntries.clear();
		tempEntries.push_back(PatchEngine::PatchEntry(PatchEngine::PATCH_8BIT, 0x00000000, 0x00000000));
	}
	else
	{
	    currentName = wxString::FromAscii(onFrame.at(_selection).name.c_str());
	    tempEntries = onFrame.at(_selection).entries;
	}

	itCurEntry = tempEntries.begin();

	wxBoxSizer* sEditPatch = new wxBoxSizer(wxVERTICAL);
	wxStaticText* EditPatchNameText = new wxStaticText(this, ID_EDITPATCH_NAME_TEXT, _("Name:"), wxDefaultPosition, wxDefaultSize);
	EditPatchName = new wxTextCtrl(this, ID_EDITPATCH_NAME, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	EditPatchName->SetValue(currentName);
	wxStaticText* EditPatchOffsetText = new wxStaticText(this, ID_EDITPATCH_OFFSET_TEXT, _("Offset:"), wxDefaultPosition, wxDefaultSize);
	EditPatchOffset = new wxTextCtrl(this, ID_EDITPATCH_OFFSET, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	EditPatchOffset->SetValue(wxString::Format(wxT("%08X"), tempEntries.at(0).address));
	EntrySelection = new wxSpinButton(this, ID_ENTRY_SELECT, wxDefaultPosition, wxDefaultSize, wxVERTICAL);
	EntrySelection->SetRange(0, (int)tempEntries.size()-1);
	EntrySelection->SetValue((int)tempEntries.size()-1);
	wxArrayString wxArrayStringFor_EditPatchType;
	for (int i = 0; i < 3; ++i)
		wxArrayStringFor_EditPatchType.Add(wxString::FromAscii(PatchEngine::PatchTypeStrings[i]));
	EditPatchType = new wxRadioBox(this, ID_EDITPATCH_TYPE, _("Type"), wxDefaultPosition, wxDefaultSize, wxArrayStringFor_EditPatchType, 3, wxRA_SPECIFY_COLS, wxDefaultValidator);
	EditPatchType->SetSelection((int)tempEntries.at(0).type);
	wxStaticText* EditPatchValueText = new wxStaticText(this, ID_EDITPATCH_VALUE_TEXT, _("Value:"), wxDefaultPosition, wxDefaultSize);
	EditPatchValue = new wxTextCtrl(this, ID_EDITPATCH_VALUE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0);
	EditPatchValue->SetValue(wxString::Format(wxT("%08X"), tempEntries.at(0).value));
	wxButton *EntryAdd = new wxButton(this, ID_ENTRY_ADD, _("Add"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	EntryRemove = new wxButton(this, ID_ENTRY_REMOVE, _("Remove"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	if ((int)tempEntries.size() <= 1)
		EntryRemove->Disable();

	wxBoxSizer* sEditPatchName = new wxBoxSizer(wxHORIZONTAL);
	sEditPatchName->Add(EditPatchNameText, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sEditPatchName->Add(EditPatchName, 1, wxEXPAND|wxALL, 5);
	sEditPatch->Add(sEditPatchName, 0, wxEXPAND);
	sbEntry = new wxStaticBoxSizer(wxVERTICAL, this, wxString::Format(wxT("Entry 1/%d"), (int)tempEntries.size()));
	currentItem = 1;
	wxGridBagSizer* sgEntry = new wxGridBagSizer(0, 0);
	sgEntry->AddGrowableCol(1);
	sgEntry->Add(EditPatchType, wxGBPosition(0, 0), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sgEntry->Add(EditPatchOffsetText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sgEntry->Add(EditPatchOffset, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);	
	sgEntry->Add(EditPatchValueText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sgEntry->Add(EditPatchValue, wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sgEntry->Add(EntrySelection, wxGBPosition(0, 2), wxGBSpan(3, 1), wxEXPAND|wxALL, 5);
	wxBoxSizer* sEntryAddRemove = new wxBoxSizer(wxHORIZONTAL);
	sEntryAddRemove->Add(EntryAdd, 0, wxALL, 5);
	sEntryAddRemove->Add(EntryRemove, 0, wxALL, 5);
	sbEntry->Add(sgEntry, 0, wxEXPAND);
	sbEntry->Add(sEntryAddRemove, 0, wxEXPAND);
	sEditPatch->Add(sbEntry, 0, wxEXPAND|wxALL, 5);
	wxBoxSizer* sEditPatchButtons = new wxBoxSizer(wxHORIZONTAL);
	wxButton* bOK = new wxButton(this, wxID_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	wxButton* bCancel = new wxButton(this, wxID_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0, wxDefaultValidator);
	sEditPatchButtons->Add(0, 0, 1, wxEXPAND, 5);
	sEditPatchButtons->Add(bOK, 0, wxALL, 5);
	sEditPatchButtons->Add(bCancel, 0, wxALL, 5);
	sEditPatch->Add(sEditPatchButtons, 0, wxEXPAND, 5);
	this->SetSizer(sEditPatch);
	sEditPatch->Layout();
	Fit();
}

void CPatchAddEdit::OnClose(wxCloseEvent& WXUNUSED (event))
{
	Destroy();
}

void CPatchAddEdit::ChangeEntry(wxSpinEvent& event)
{
	SaveEntryData(itCurEntry);
	
	itCurEntry = tempEntries.end() - event.GetPosition() - 1;
	currentItem = (int)tempEntries.size() - event.GetPosition();
	UpdateEntryCtrls(*itCurEntry);
}

void CPatchAddEdit::SavePatchData(wxCommandEvent& WXUNUSED (event))
{
	SaveEntryData(itCurEntry);

	if (selection == -1)
	{
		PatchEngine::Patch newPatch;
		newPatch.name = std::string(EditPatchName->GetValue().mb_str());
		newPatch.entries = tempEntries;
		newPatch.active = true;

		onFrame.push_back(newPatch);
	}
	else
	{
		onFrame.at(selection).name = std::string(EditPatchName->GetValue().mb_str());
		onFrame.at(selection).entries = tempEntries;
	}

	AcceptAndClose();
}

void CPatchAddEdit::AddRemoveEntry(wxCommandEvent& event)
{
	switch (event.GetId())
	{
	case ID_ENTRY_ADD:
		{
			SaveEntryData(itCurEntry);

			PatchEngine::PatchEntry peEmptyEntry(PatchEngine::PATCH_8BIT, 0x00000000, 0x00000000);
			itCurEntry++;
			currentItem++;
			itCurEntry = tempEntries.insert(itCurEntry, peEmptyEntry);

			EntrySelection->SetRange(EntrySelection->GetMin(), EntrySelection->GetMax() + 1);
			UpdateEntryCtrls(*itCurEntry);

			EntryRemove->Enable();
			EntrySelection->Enable();
		}
		break;
	case ID_ENTRY_REMOVE:
		{		
			itCurEntry = tempEntries.erase(itCurEntry);
			
			if (itCurEntry != tempEntries.begin())
			{
				itCurEntry--;
				currentItem--;
			}
			else
			{
				EntrySelection->SetValue(EntrySelection->GetValue() - 1);
			}
			
			EntrySelection->SetRange(EntrySelection->GetMin(), EntrySelection->GetMax() - 1);			
			UpdateEntryCtrls(*itCurEntry);

			if ((int)tempEntries.size() <= 1)
			{
				EntryRemove->Disable();
				EntrySelection->Disable();
			}
		}
		break;
	}
}

void CPatchAddEdit::UpdateEntryCtrls(PatchEngine::PatchEntry pE)
{
	sbEntry->GetStaticBox()->SetLabel(wxString::Format(wxT("Entry %d/%d"), currentItem,
									  (int)tempEntries.size()));
	EditPatchOffset->SetValue(wxString::Format(wxT("%08X"), pE.address));
	EditPatchType->SetSelection(pE.type);
	EditPatchValue->SetValue(wxString::Format(wxT("%08X"), pE.value));
}

void CPatchAddEdit::SaveEntryData(std::vector<PatchEngine::PatchEntry>::iterator iterEntry)
{
	unsigned long value;

	if (EditPatchOffset->GetValue().ToULong(&value, 16))
		(*iterEntry).address = value;
	(*iterEntry).type = (PatchEngine::PatchType) EditPatchType->GetSelection();
	if (EditPatchValue->GetValue().ToULong(&value, 16))
		(*iterEntry).value = value;
}
