// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <vector>

#include <wx/button.h>
#include <wx/gbsizer.h>
#include <wx/msgdlg.h>
#include <wx/radiobox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/CommonTypes.h"
#include "Core/HideObjectEngine.h"
#include "DolphinWX/HideObjectAddEdit.h"
#include "DolphinWX/WxUtils.h"

CHideObjectAddEdit::CHideObjectAddEdit(int _selection, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	selection = _selection;
	CreateGUIControls(selection);

	Bind(wxEVT_BUTTON, &CHideObjectAddEdit::SaveHideObjectData, this, wxID_OK);
}

CHideObjectAddEdit::~CHideObjectAddEdit()
{
}

void CHideObjectAddEdit::CreateGUIControls(int _selection)
{
	wxString currentName = _("<Insert name here>");

	if (_selection == -1)
	{
		tempEntries.clear();
		tempEntries.emplace_back(HideObjectEngine::HideObject_16BIT, 0x00000000);
	}
	else
	{
		currentName = StrToWxStr(HideObjectCodes[_selection].name);
		tempEntries = HideObjectCodes[_selection].entries;
	}

	itCurEntry = tempEntries.begin();

	wxBoxSizer* sEditHideObject = new wxBoxSizer(wxVERTICAL);

	wxStaticText* EditHideObjectNameText = new wxStaticText(this, wxID_ANY, _("Name:"));
	EditHideObjectName = new wxTextCtrl(this, wxID_ANY);
	EditHideObjectName->SetValue(currentName);

	wxArrayString wxArrayStringFor_EditHideObjectType;
	for (int i = 0; i < 16; ++i)
		wxArrayStringFor_EditHideObjectType.Add(StrToWxStr(HideObjectEngine::HideObjectTypeStrings[i]));
	EditHideObjectType = new wxRadioBox(this, wxID_ANY, _("Size"), wxDefaultPosition, wxDefaultSize, wxArrayStringFor_EditHideObjectType, 8, wxRA_SPECIFY_COLS);
	EditHideObjectType->SetSelection((int)tempEntries[0].type);
	EditHideObjectType->Bind(wxEVT_RADIOBOX, &CHideObjectAddEdit::ButtonSize, this);

	wxStaticText* EditHideObjectValueText = new wxStaticText(this, wxID_ANY, _("Value:"));
	EditHideObjectValue = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(225, 20));
	if (HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type) <= 8)
		EditHideObjectValue->SetValue(wxString::Format("%0*X", HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type), (u32)tempEntries[0].value_lower));
	else if (HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type) <= 16)
		EditHideObjectValue->SetValue(wxString::Format("%0*X%08X", HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type)-8, (u32)((tempEntries[0].value_lower & 0xffffffff00000000) >> 32), (u32)((tempEntries[0].value_lower & 0xffffffff))));
	else if (HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type) <= 24)
		EditHideObjectValue->SetValue(wxString::Format("%0*X%08X%08X", HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type)-16, (u32)(tempEntries[0].value_upper & 0xffffffff), (u32)((tempEntries[0].value_lower & 0xffffffff00000000) >> 32), (u32)((tempEntries[0].value_lower & 0xffffffff))));
	else if (HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type) <= 32)
		EditHideObjectValue->SetValue(wxString::Format("%0*X%08X%08X%08X", HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type)-24, (u32)((tempEntries[0].value_upper & 0xffffffff00000000) >> 32), (u32)(tempEntries[0].value_upper & 0xffffffff), (u32)((tempEntries[0].value_lower & 0xffffffff00000000) >> 32), (u32)((tempEntries[0].value_lower & 0xffffffff))));

	wxButton* BruteForceUp = new wxButton(this, ID_BUTTON_UP, _("Up"));
	wxButton* BruteForceDown = new wxButton(this, ID_BUTTON_DOWN, _("Down"));
	wxString* BruteForceToolTip = new wxString(_("The up or down button can be used to find new codes.  While the game is playing, find an object you want to hide and select '8bits'.  Keep clicking the up button until the object disappears.  Now choose '16bits' and "
		"continue clicking up until the object is hidden again.  Repeat this process until the code is long enough to be unique.  Warning: Too short of a code may result in other objects being hidden."));
	BruteForceUp->SetToolTip(*BruteForceToolTip);
	BruteForceDown->SetToolTip(*BruteForceToolTip);
	BruteForceUp->Bind(wxEVT_BUTTON, &CHideObjectAddEdit::ButtonUporDown, this);
	BruteForceDown->Bind(wxEVT_BUTTON, &CHideObjectAddEdit::ButtonUporDown, this);

	wxBoxSizer* sEditHideObjectName = new wxBoxSizer(wxHORIZONTAL);
	sEditHideObjectName->Add(EditHideObjectNameText, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sEditHideObjectName->Add(EditHideObjectName, 1, wxEXPAND|wxALL, 5);
	sEditHideObject->Add(sEditHideObjectName, 0, wxEXPAND);
	sbEntry = new wxStaticBoxSizer(wxVERTICAL, this, wxString::Format(_("Hide Objects Code")));

	wxGridBagSizer* sgEntry = new wxGridBagSizer(0, 0);
	sgEntry->Add(EditHideObjectType, wxGBPosition(0, 0), wxGBSpan(2, 5), wxEXPAND|wxALL, 5);
	sgEntry->Add(EditHideObjectValueText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sgEntry->Add(EditHideObjectValue, wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sgEntry->Add(BruteForceUp, wxGBPosition(2, 2), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sgEntry->Add(BruteForceDown, wxGBPosition(2, 3), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);
	sgEntry->AddGrowableCol(1);

	sbEntry->Add(sgEntry, 0, wxEXPAND);

	sEditHideObject->Add(sbEntry, 0, wxEXPAND|wxALL, 5);
	sEditHideObject->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5);
	SetSizerAndFit(sEditHideObject);
	SetFocus();
}

void CHideObjectAddEdit::SaveHideObjectData(wxCommandEvent& event)
{
	if (!UpdateTempEntryData(itCurEntry))
		return;

	if (selection == -1)
	{
		for (HideObjectEngine::HideObject& code : HideObjectCodes)
		{
			if (code.name == WxStrToStr(EditHideObjectName->GetValue()))
			{
				wxMessageBox(_("Name is already in use.  Please choose a unique name."), _("Error"));
				return;
			}
		}

		HideObjectEngine::HideObject newHideObject;
		newHideObject.name = WxStrToStr(EditHideObjectName->GetValue());
		newHideObject.entries = tempEntries;
		newHideObject.active = true;

		HideObjectCodes.push_back(newHideObject);
	}
	else
	{
		HideObjectCodes[selection].name = WxStrToStr(EditHideObjectName->GetValue());
		HideObjectCodes[selection].entries = tempEntries;
	}

	AcceptAndClose();
	event.Skip();
}

// Change the number of bits by adding or removing digits on the right hand side of the string.
void CHideObjectAddEdit::ButtonSize(wxCommandEvent& event)
{
	wxString s = EditHideObjectValue->GetValue();
	size_t old_length = s.Length();
	HideObjectEngine::HideObjectType temp_type = (HideObjectEngine::HideObjectType)EditHideObjectType->GetSelection();
	size_t new_length = HideObjectEngine::GetHideObjectTypeCharLength(temp_type);
	if (new_length <= old_length)
		s = s.Left(new_length);
	else
		s.Append('0', new_length - old_length);
	EditHideObjectValue->SetValue(s);

	ButtonUporDown(event);
}


void CHideObjectAddEdit::ButtonUporDown(wxCommandEvent& event)
{
	if (!UpdateTempEntryData(itCurEntry))
		return;

	unsigned long long value_lower;
	unsigned long long value_upper = 0;
	HideObjectEngine::HideObjectType tempType = (HideObjectEngine::HideObjectType)EditHideObjectType->GetSelection();

	// Get wxString
	wxString valueString = EditHideObjectValue->GetValue();
	int length = EditHideObjectValue->GetValue().length();

	// Parse Upper If Needed
	if (tempType > HideObjectEngine::HideObject_64BIT)
	{
		if (length > 16)
		{
			if (EditHideObjectValue->GetValue().Left(length - 16).ToULongLong(&value_upper, 16))
			{
				if (ParseValue(value_upper, (HideObjectEngine::HideObjectType)(tempType - 8)))
					length = 16;
				else
					return;
			}
			else
			{
				wxMessageBox(_("Invalid or non hex (0-9 or A-F) character entered.\nEntry not modified."), _("Error"));
				return;
			}
		}
	}
	// Always Parse Lower
	if (EditHideObjectValue->GetValue().Right(length).ToULongLong(&value_lower, 16))
	{
		if (!ParseValue(value_lower, tempType))
			return;
	}
	else
	{
		wxMessageBox(_("Invalid or non hex (0-9 or A-F) character entered.\nEntry not modified."), _("Error"));
		return;
	}

	// Now we have upper and lower

	if (event.GetId() == ID_BUTTON_UP)
	{
		if (value_lower == 0xFFFFFFFFFFFFFFFF)
		{
			if (value_upper == 0xFFFFFFFFFFFFFFFF)
			{
				return;
			}
			else
			{
				value_lower = 0;
				value_upper++;
			}
		}
		else
		{
			value_lower++;
		}
	}
	else if (event.GetId() == ID_BUTTON_DOWN)
	{
		if (value_lower == 0)
		{
			if (value_upper == 0)
			{
				return;
			}
			else
			{
				value_upper--;
				value_lower = 0xFFFFFFFFFFFFFFFF;
			}
		}
		else
		{
			value_lower--;
		}
	}

	// Double check values are valid before updating the entry.
	if (tempType > HideObjectEngine::HideObject_64BIT)
	{
		if (!ParseValue(value_upper, (HideObjectEngine::HideObjectType)(tempType-8)))
			return;
	}
	else if (value_upper > 0)
	{
		return;
	}

	if (!ParseValue(value_lower, tempType))
		return;

	// Print new valid entry.
	if (HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type) <= 8)
		EditHideObjectValue->SetValue(wxString::Format("%0*X", HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type), (u32)value_lower));
	else if (HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type) <= 16)
		EditHideObjectValue->SetValue(wxString::Format("%0*X%08X", HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type) - 8, (u32)((value_lower & 0xffffffff00000000) >> 32), (u32)(value_lower & 0xffffffff)));
	else if (HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type) <= 24)
		EditHideObjectValue->SetValue(wxString::Format("%0*X%08X%08X", HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type) - 16, (u32)(value_upper & 0xffffffff), (u32)((value_lower & 0xffffffff00000000) >> 32), (u32)(value_lower & 0xffffffff)));
	else if (HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type) <= 32)
		EditHideObjectValue->SetValue(wxString::Format("%0*X%08X%08X%08X", HideObjectEngine::GetHideObjectTypeCharLength(tempEntries[0].type) - 24, (u32)((value_upper & 0xffffffff00000000) >> 32), (u32)(value_upper & 0xffffffff), (u32)((value_lower & 0xffffffff00000000) >> 32), (u32)(value_lower & 0xffffffff)));

	// Put the valid entry in the HideObjectEntry
	(*itCurEntry).value_upper = value_upper;
	(*itCurEntry).value_lower = value_lower;

	if (selection == -1)
	{
		HideObjectEngine::HideObject newHideObject;

		newHideObject.entries = tempEntries;
		newHideObject.active = true;
		HideObjectCodes.push_back(newHideObject);

		HideObjectEngine::ApplyHideObjects(HideObjectCodes);

		HideObjectCodes.pop_back();
	}
	else
	{
		HideObjectCodes[selection].name = WxStrToStr(EditHideObjectName->GetValue());
		HideObjectCodes[selection].entries = tempEntries;

		HideObjectEngine::ApplyHideObjects(HideObjectCodes);
	}

	event.Skip();
}

bool CHideObjectAddEdit::UpdateTempEntryData(std::vector<HideObjectEngine::HideObjectEntry>::iterator iterEntry)
{
	unsigned long long value_lower;
	unsigned long long value_upper;

	HideObjectEngine::HideObjectType tempType =
		(*iterEntry).type = (HideObjectEngine::HideObjectType)EditHideObjectType->GetSelection();

	size_t length = EditHideObjectValue->GetValue().Length();

	// Parse Upper If Needed
	if (tempType > HideObjectEngine::HideObject_64BIT) {
		if (length > 16) {
			if (EditHideObjectValue->GetValue().Left(length - 16).ToULongLong(&value_upper, 16))
			{
				if (ParseValue(value_upper, (HideObjectEngine::HideObjectType)(tempType-8)))
				{
					(*iterEntry).value_upper = value_upper;
					length = 16;
				}
				else
				{
					return false;
				}
			}
			else
			{
				wxMessageBox(_("Invalid or non hex (0-9 or A-F) character entered.\nEntry not modified."), _("Error"));
				return false;
			}
		}
		else
		{
			value_upper = 0;
		}
	}
	// Always Parse Lower
	if (EditHideObjectValue->GetValue().Right(length).ToULongLong(&value_lower, 16))
	{
		if (ParseValue(value_lower, tempType))
		{
			(*iterEntry).value_lower = value_lower;
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		wxMessageBox(_("Invalid or non hex (0-9 or A-F) character entered.\nEntry not modified."), _("Error"));
		return false;
	}
}


// Checks if the value is larger than the maximum it is allowed by the type.
// e.g. if tempType = 8bits, value cannot be > 0xFF.
bool CHideObjectAddEdit::ParseValue(unsigned long long value, HideObjectEngine::HideObjectType tempType)
{
	if (!(tempType >= 7 || !(value >> ((tempType + 1ull) << 3ull))))
	{
		wxMessageBox(_("Value too large.\nEntry not modified."), _("Error"));
		return false;
	}

	return true;
}
