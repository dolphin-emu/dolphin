// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>
#include <vector>
#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/chartype.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gbsizer.h>
#include <wx/gdicmn.h>
#include <wx/msgdlg.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/spinbutt.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
#include "Core/PatchEngine.h"
#include "DolphinWX/PatchAddEdit.h"
#include "DolphinWX/WxUtils.h"

CPatchAddEdit::CPatchAddEdit(int _selection, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	selection = _selection;
	CreateGUIControls(selection);

	Bind(wxEVT_BUTTON, &CPatchAddEdit::SavePatchData, this, wxID_OK);
}

CPatchAddEdit::~CPatchAddEdit()
{
}

void CPatchAddEdit::CreateGUIControls(int _selection)
{
	wxString currentName = _("<Insert name here>");

	if (_selection == -1)
	{
		tempEntries.clear();
		tempEntries.push_back(PatchEngine::PatchEntry(PatchEngine::PATCH_8BIT, 0x00000000, 0x00000000));
	}
	else
	{
		currentName = StrToWxStr(onFrame.at(_selection).name);
		tempEntries = onFrame.at(_selection).entries;
	}

	itCurEntry = tempEntries.begin();

	wxBoxSizer* sEditPatch = new wxBoxSizer(wxVERTICAL);

	wxStaticText* EditPatchNameText = new wxStaticText(this, wxID_ANY, _("Name:"));
	EditPatchName = new wxTextCtrl(this, wxID_ANY);
	EditPatchName->SetValue(currentName);

	wxStaticText* EditPatchOffsetText = new wxStaticText(this, wxID_ANY, _("Offset:"));
	EditPatchOffset = new wxTextCtrl(this, wxID_ANY);
	EditPatchOffset->SetValue(wxString::Format("%08X", tempEntries.at(0).address));

	EntrySelection = new wxSpinButton(this);
	EntrySelection->Bind(wxEVT_SPIN, &CPatchAddEdit::ChangeEntry, this);
	EntrySelection->SetRange(0, (int)tempEntries.size()-1);
	EntrySelection->SetValue((int)tempEntries.size()-1);

	wxArrayString wxArrayStringFor_EditPatchType;
	for (int i = 0; i < 3; ++i)
		wxArrayStringFor_EditPatchType.Add(StrToWxStr(PatchEngine::PatchTypeStrings[i]));
	EditPatchType = new wxRadioBox(this, wxID_ANY, _("Type"), wxDefaultPosition, wxDefaultSize, wxArrayStringFor_EditPatchType, 3, wxRA_SPECIFY_COLS);
	EditPatchType->SetSelection((int)tempEntries.at(0).type);

	wxStaticText* EditPatchValueText = new wxStaticText(this, wxID_ANY, _("Value:"));
	EditPatchValue = new wxTextCtrl(this, wxID_ANY);
	EditPatchValue->SetValue(wxString::Format("%0*X", PatchEngine::GetPatchTypeCharLength(tempEntries.at(0).type), tempEntries.at(0).value));

	EntryAdd = new wxButton(this, wxID_ANY, _("Add"));
	EntryAdd->Bind(wxEVT_BUTTON, &CPatchAddEdit::AddEntry, this);

	EntryRemove = new wxButton(this, wxID_ANY, _("Remove"));
	EntryRemove->Bind(wxEVT_BUTTON, &CPatchAddEdit::RemoveEntry, this);
	if ((int)tempEntries.size() <= 1)
		EntryRemove->Disable();

	wxBoxSizer* sEditPatchName = new wxBoxSizer(wxHORIZONTAL);
	sEditPatchName->Add(EditPatchNameText, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sEditPatchName->Add(EditPatchName, 1, wxEXPAND|wxALL, 5);
	sEditPatch->Add(sEditPatchName, 0, wxEXPAND);
	sbEntry = new wxStaticBoxSizer(wxVERTICAL, this, wxString::Format(_("Entry 1/%d"), (int)tempEntries.size()));
	currentItem = 1;

	wxGridBagSizer* sgEntry = new wxGridBagSizer(0, 0);
	sgEntry->Add(EditPatchType, wxGBPosition(0, 0), wxGBSpan(1, 2), wxEXPAND|wxALL, 5);
	sgEntry->Add(EditPatchOffsetText, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sgEntry->Add(EditPatchOffset, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sgEntry->Add(EditPatchValueText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sgEntry->Add(EditPatchValue, wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sgEntry->Add(EntrySelection, wxGBPosition(0, 2), wxGBSpan(3, 1), wxEXPAND|wxALL, 5);
	sgEntry->AddGrowableCol(1);

	wxBoxSizer* sEntryAddRemove = new wxBoxSizer(wxHORIZONTAL);
	sEntryAddRemove->Add(EntryAdd, 0, wxALL, 5);
	sEntryAddRemove->Add(EntryRemove, 0, wxALL, 5);
	sbEntry->Add(sgEntry, 0, wxEXPAND);
	sbEntry->Add(sEntryAddRemove, 0, wxEXPAND);

	sEditPatch->Add(sbEntry, 0, wxEXPAND|wxALL, 5);
	sEditPatch->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5);
	SetSizerAndFit(sEditPatch);
	SetFocus();
}

void CPatchAddEdit::ChangeEntry(wxSpinEvent& event)
{
	if (!UpdateTempEntryData(itCurEntry))
		return;

	itCurEntry = tempEntries.end() - event.GetPosition() - 1;
	currentItem = (int)tempEntries.size() - event.GetPosition();
	UpdateEntryCtrls(*itCurEntry);
}

void CPatchAddEdit::SavePatchData(wxCommandEvent& event)
{
	if (!UpdateTempEntryData(itCurEntry))
		return;

	if (selection == -1)
	{
		PatchEngine::Patch newPatch;
		newPatch.name = WxStrToStr(EditPatchName->GetValue());
		newPatch.entries = tempEntries;
		newPatch.active = true;

		onFrame.push_back(newPatch);
	}
	else
	{
		onFrame.at(selection).name = WxStrToStr(EditPatchName->GetValue());
		onFrame.at(selection).entries = tempEntries;
	}

	AcceptAndClose();
	event.Skip();
}

void CPatchAddEdit::AddEntry(wxCommandEvent& event)
{
	if (!UpdateTempEntryData(itCurEntry))
		return;

	PatchEngine::PatchEntry peEmptyEntry(PatchEngine::PATCH_8BIT, 0x00000000, 0x00000000);
	++itCurEntry;
	currentItem++;
	itCurEntry = tempEntries.insert(itCurEntry, peEmptyEntry);

	EntrySelection->SetRange(EntrySelection->GetMin(), EntrySelection->GetMax() + 1);
	UpdateEntryCtrls(*itCurEntry);

	EntryRemove->Enable();
	EntrySelection->Enable();
}

void CPatchAddEdit::RemoveEntry(wxCommandEvent& event)
{
	itCurEntry = tempEntries.erase(itCurEntry);

	if (itCurEntry != tempEntries.begin())
	{
		--itCurEntry;
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

void CPatchAddEdit::UpdateEntryCtrls(PatchEngine::PatchEntry pE)
{
	sbEntry->GetStaticBox()->SetLabel(wxString::Format(_("Entry %d/%d"), currentItem,
									  (int)tempEntries.size()));
	EditPatchOffset->SetValue(wxString::Format("%08X", pE.address));
	EditPatchType->SetSelection(pE.type);
	EditPatchValue->SetValue(wxString::Format("%0*X",
		PatchEngine::GetPatchTypeCharLength(pE.type), pE.value));
}

bool CPatchAddEdit::UpdateTempEntryData(std::vector<PatchEngine::PatchEntry>::iterator iterEntry)
{
	unsigned long value;
	bool parsed_ok = true;

	if (EditPatchOffset->GetValue().ToULong(&value, 16))
		(*iterEntry).address = value;
	else
		parsed_ok = false;

	PatchEngine::PatchType tempType =
	(*iterEntry).type = (PatchEngine::PatchType)EditPatchType->GetSelection();

	if (EditPatchValue->GetValue().ToULong(&value, 16))
	{
		(*iterEntry).value = value;
		if (tempType == PatchEngine::PATCH_8BIT && value > 0xff)
			parsed_ok = false;
		else if (tempType == PatchEngine::PATCH_16BIT && value > 0xffff)
			parsed_ok = false;
	}
	else
	{
		parsed_ok = false;
	}

	if (!parsed_ok)
	{
		wxMessageBox(_("Unable to create patch from given values.\nEntry not modified."), _("Error"));
	}

	return parsed_ok;
}
