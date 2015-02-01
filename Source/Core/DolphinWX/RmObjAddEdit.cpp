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
#include "Core/RmObjEngine.h"
#include "DolphinWX/RmObjAddEdit.h"
#include "DolphinWX/WxUtils.h"

CRmObjAddEdit::CRmObjAddEdit(int _selection, wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& position, const wxSize& size, long style)
	: wxDialog(parent, id, title, position, size, style)
{
	selection = _selection;
	CreateGUIControls(selection);

	Bind(wxEVT_BUTTON, &CRmObjAddEdit::SaveRmObjData, this, wxID_OK);
}

CRmObjAddEdit::~CRmObjAddEdit()
{
}

void CRmObjAddEdit::CreateGUIControls(int _selection)
{
	wxString currentName = _("<Insert name here>");

	if (_selection == -1)
	{
		tempEntries.clear();
		tempEntries.push_back(RmObjEngine::RmObjEntry(RmObjEngine::RMOBJ_16BIT, 0x00000000));
	}
	else
	{
		currentName = StrToWxStr(rmObjCodes.at(_selection).name);
		tempEntries = rmObjCodes.at(_selection).entries;
	}

	itCurEntry = tempEntries.begin();

	wxBoxSizer* sEditRmObj = new wxBoxSizer(wxVERTICAL);

	wxStaticText* EditRmObjNameText = new wxStaticText(this, wxID_ANY, _("Name:"));
	EditRmObjName = new wxTextCtrl(this, wxID_ANY);
	EditRmObjName->SetValue(currentName);

	wxArrayString wxArrayStringFor_EditRmObjType;
	for (int i = 0; i < 16; ++i)
		wxArrayStringFor_EditRmObjType.Add(StrToWxStr(RmObjEngine::RmObjTypeStrings[i]));
	EditRmObjType = new wxRadioBox(this, wxID_ANY, _("Size"), wxDefaultPosition, wxDefaultSize, wxArrayStringFor_EditRmObjType, 8, wxRA_SPECIFY_COLS);
	EditRmObjType->SetSelection((int)tempEntries.at(0).type);

	wxStaticText* EditRmObjValueText = new wxStaticText(this, wxID_ANY, _("Value:"));
	EditRmObjValue = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(225, 20));
	if (RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type) <= 8)
		EditRmObjValue->SetValue(wxString::Format("%0*X", RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type), (u32)tempEntries.at(0).value_lower));
	else if (RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type) <= 16)
		EditRmObjValue->SetValue(wxString::Format("%0*X%08X", RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type)-8, (u32)((tempEntries.at(0).value_lower & 0xffffffff00000000) >> 32), (u32)((tempEntries.at(0).value_lower & 0xffffffff))));
	else if (RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type) <= 24)
		EditRmObjValue->SetValue(wxString::Format("%0*X%08X%08X", RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type)-16, (u32)(tempEntries.at(0).value_upper & 0xffffffff), (u32)((tempEntries.at(0).value_lower & 0xffffffff00000000) >> 32), (u32)((tempEntries.at(0).value_lower & 0xffffffff))));
	else if (RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type) <= 32)
		EditRmObjValue->SetValue(wxString::Format("%0*X%08X%08X%08X", RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type)-24, (u32)((tempEntries.at(0).value_upper & 0xffffffff00000000) >> 32), (u32)(tempEntries.at(0).value_upper & 0xffffffff), (u32)((tempEntries.at(0).value_lower & 0xffffffff00000000) >> 32), (u32)((tempEntries.at(0).value_lower & 0xffffffff))));

	wxButton* BruteForceUp = new wxButton(this, ID_BUTTON_UP, _("Up"));
	wxButton* BruteForceDown = new wxButton(this, ID_BUTTON_DOWN, _("Down"));
	//BruteForceUp->SetToolTip(_(""));
	//BruteForceDown->SetToolTip(_(""));
	BruteForceUp->Bind(wxEVT_BUTTON, &CRmObjAddEdit::ButtonUporDown, this);
	BruteForceDown->Bind(wxEVT_BUTTON, &CRmObjAddEdit::ButtonUporDown, this);

	wxBoxSizer* sEditRmObjName = new wxBoxSizer(wxHORIZONTAL);
	sEditRmObjName->Add(EditRmObjNameText, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sEditRmObjName->Add(EditRmObjName, 1, wxEXPAND|wxALL, 5);
	sEditRmObj->Add(sEditRmObjName, 0, wxEXPAND);
	sbEntry = new wxStaticBoxSizer(wxVERTICAL, this, wxString::Format(_("Object Removal Code")));

	wxGridBagSizer* sgEntry = new wxGridBagSizer(0, 0);
	sgEntry->Add(EditRmObjType, wxGBPosition(0, 0), wxGBSpan(2, 5), wxEXPAND|wxALL, 5);
	sgEntry->Add(EditRmObjValueText, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL|wxALL, 5);
	sgEntry->Add(EditRmObjValue, wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sgEntry->Add(BruteForceUp, wxGBPosition(2, 2), wxGBSpan(1, 1), wxEXPAND|wxALL, 5);
	sgEntry->Add(BruteForceDown, wxGBPosition(2, 3), wxGBSpan(1, 1), wxEXPAND | wxALL, 5);
	sgEntry->AddGrowableCol(1);

	sbEntry->Add(sgEntry, 0, wxEXPAND);

	sEditRmObj->Add(sbEntry, 0, wxEXPAND|wxALL, 5);
	sEditRmObj->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5);
	SetSizerAndFit(sEditRmObj);
	SetFocus();
}

void CRmObjAddEdit::SaveRmObjData(wxCommandEvent& event)
{
	if (!UpdateTempEntryData(itCurEntry))
		return;
		
	if (selection == -1)
	{
		for (int i = 0; i < rmObjCodes.size(); ++i)
		{
			if (rmObjCodes.at(i).name == WxStrToStr(EditRmObjName->GetValue()))
			{
				wxMessageBox(_("Name is already in use.  Please choose a unique name."), _("Error"));
				return;
			}
		}

		RmObjEngine::RmObj newRmObj;
		newRmObj.name = WxStrToStr(EditRmObjName->GetValue());
		newRmObj.entries = tempEntries;
		newRmObj.active = true;

		rmObjCodes.push_back(newRmObj);
	}
	else
	{
		rmObjCodes.at(selection).name = WxStrToStr(EditRmObjName->GetValue());
		rmObjCodes.at(selection).entries = tempEntries;
	}

	AcceptAndClose();
	event.Skip();
}

void CRmObjAddEdit::ButtonUporDown(wxCommandEvent& event)
{
	if (!UpdateTempEntryData(itCurEntry))
		return;

	unsigned long long value_lower;
	unsigned long long value_upper = 0;
	RmObjEngine::RmObjType tempType = (RmObjEngine::RmObjType)EditRmObjType->GetSelection();

	//Get wxString
	wxString valueString = EditRmObjValue->GetValue();
	int length = EditRmObjValue->GetValue().length();

	//Parse Upper If Needed
	if (tempType > RmObjEngine::RMOBJ_64BIT) 
	{
		if (length > 16) 
		{
			if (EditRmObjValue->GetValue().Left(length - 16).ToULongLong(&value_upper, 16)) 
			{
				if (ParseValue(value_upper, (RmObjEngine::RmObjType)(tempType - 8)))
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
	//Always Parse Lower
	if (EditRmObjValue->GetValue().Right(length).ToULongLong(&value_lower, 16)) 
	{
		if (!ParseValue(value_lower, tempType))
			return;
	}
	else 
	{
		wxMessageBox(_("Invalid or non hex (0-9 or A-F) character entered.\nEntry not modified."), _("Error"));
		return;
	}

	//Now we have upper and lower

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
	else 
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
	if (tempType > RmObjEngine::RMOBJ_64BIT) 
	{
		if (!ParseValue(value_upper, (RmObjEngine::RmObjType)(tempType-8)))
			return;
	}
	else if (value_upper > 0) 
	{
			return;
	}

	if (!ParseValue(value_lower, tempType))
		return;

	// Print new valid entry.
	if (RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type) <= 8)
		EditRmObjValue->SetValue(wxString::Format("%0*X", RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type), (u32)value_lower));
	else if (RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type) <= 16)
		EditRmObjValue->SetValue(wxString::Format("%0*X%08X", RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type) - 8, (u32)((value_lower & 0xffffffff00000000) >> 32), (u32)(value_lower & 0xffffffff)));
	else if (RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type) <= 24)
		EditRmObjValue->SetValue(wxString::Format("%0*X%08X%08X", RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type) - 16, (u32)(value_upper & 0xffffffff), (u32)((value_lower & 0xffffffff00000000) >> 32), (u32)(value_lower & 0xffffffff)));
	else if (RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type) <= 32)
		EditRmObjValue->SetValue(wxString::Format("%0*X%08X%08X%08X", RmObjEngine::GetRmObjTypeCharLength(tempEntries.at(0).type) - 24, (u32)((value_upper & 0xffffffff00000000) >> 32), (u32)(value_upper & 0xffffffff), (u32)((value_lower & 0xffffffff00000000) >> 32), (u32)(value_lower & 0xffffffff)));

	// Put the valid entry in the RmObjEntry
	(*itCurEntry).value_upper = value_upper;
	(*itCurEntry).value_lower = value_lower;

	if (selection == -1)
	{
		RmObjEngine::RmObj newRmObj;
		//newRmObj.name = WxStrToStr(EditRmObjName->GetValue());
		newRmObj.entries = tempEntries;
		newRmObj.active = true;
		rmObjCodes.push_back(newRmObj);

		RmObjEngine::ApplyRmObjs(rmObjCodes);

		rmObjCodes.pop_back();
	}
	else
	{
		rmObjCodes.at(selection).name = WxStrToStr(EditRmObjName->GetValue());
		rmObjCodes.at(selection).entries = tempEntries;

		RmObjEngine::ApplyRmObjs(rmObjCodes);
	}

	event.Skip();
}

bool CRmObjAddEdit::UpdateTempEntryData(std::vector<RmObjEngine::RmObjEntry>::iterator iterEntry)
{
	unsigned long long value_lower;
	unsigned long long value_upper;

	RmObjEngine::RmObjType tempType =
		(*iterEntry).type = (RmObjEngine::RmObjType)EditRmObjType->GetSelection();

	size_t length = EditRmObjValue->GetValue().Length();

	//Parse Upper If Needed
	if (tempType > RmObjEngine::RMOBJ_64BIT) {
		if (length > 16) {
			if (EditRmObjValue->GetValue().Left(length - 16).ToULongLong(&value_upper, 16)) 
			{
				if (ParseValue(value_upper, (RmObjEngine::RmObjType)(tempType-8)))
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
	//Always Parse Lower
	if (EditRmObjValue->GetValue().Right(length).ToULongLong(&value_lower, 16))
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


//Checks if the value is larger than the maximum it is allowed by the type.
//e.g. if tempType = 8bits, value cannot be > 0xFF.
bool CRmObjAddEdit::ParseValue(unsigned long long value, RmObjEngine::RmObjType tempType)
{
	bool parsed_ok = true;

	//u64 mask = (1 << ((tempType + 1) << 3)) - 1;
	
	if (tempType == RmObjEngine::RMOBJ_08BIT && value > 0xff)
		parsed_ok = false;
	else if (tempType == RmObjEngine::RMOBJ_16BIT && value > 0xffff)
		parsed_ok = false;
	else if (tempType == RmObjEngine::RMOBJ_24BIT && value > 0xffffff)
		parsed_ok = false;
	else if (tempType == RmObjEngine::RMOBJ_32BIT && value > 0xffffffff)
		parsed_ok = false;
	else if (tempType == RmObjEngine::RMOBJ_40BIT && value > 0xffffffffff)
		parsed_ok = false;
	else if (tempType == RmObjEngine::RMOBJ_48BIT && value > 0xffffffffffff)
		parsed_ok = false;
	else if (tempType == RmObjEngine::RMOBJ_52BIT && value > 0xffffffffffffff)
		parsed_ok = false;
	else if (tempType == RmObjEngine::RMOBJ_64BIT && value > 0xffffffffffffffff)
		parsed_ok = false;
	
	if (!parsed_ok) 
	{
		wxMessageBox(_("Value too large.\nEntry not modified."), _("Error"));
		return false;
	}

	return true;
}
