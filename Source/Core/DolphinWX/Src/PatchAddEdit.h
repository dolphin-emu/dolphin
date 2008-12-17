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

#ifndef __PATCH_ADDEDIT_h__
#define __PATCH_ADDEDIT_h__

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include "ISOProperties.h"

class CPatchAddEdit : public wxDialog
{
	public:
		CPatchAddEdit(int _selection, wxWindow* parent,
			wxWindowID id = 1,
			const wxString& title = wxT("Edit Patch"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxSize(300, -1),
			long style = wxDEFAULT_DIALOG_STYLE);
		virtual ~CPatchAddEdit();

	private:
		DECLARE_EVENT_TABLE();

		wxTextCtrl *EditPatchName;
		wxTextCtrl *EditPatchOffset;
		wxRadioBox *EditPatchType;
		wxTextCtrl *EditPatchValue;
		wxSpinButton *EntrySelection;

		enum {
			ID_EDITPATCH_NAME_TEXT = 4500,
			ID_EDITPATCH_NAME,
			ID_EDITPATCH_OFFSET_TEXT,
			ID_EDITPATCH_OFFSET,
			ID_ENTRY_SELECT,
			ID_EDITPATCH_TYPE,
			ID_EDITPATCH_VALUE_TEXT,
			ID_EDITPATCH_VALUE
		};

		void CreateGUIControls(int selection);
		void OnClose(wxCloseEvent& event);
		void ChangeEntry(wxSpinEvent& event);

		int selection;

};
#endif // __PATCH_ADDEDIT_h__
