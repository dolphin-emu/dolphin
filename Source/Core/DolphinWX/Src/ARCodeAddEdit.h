// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef __ARCODE_ADDEDIT_h__
#define __ARCODE_ADDEDIT_h__

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include "ISOProperties.h"

class CARCodeAddEdit : public wxDialog
{
	public:
		CARCodeAddEdit(int _selection, wxWindow* parent,
			wxWindowID id = 1,
			const wxString& title = _("Edit ActionReplay Code"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE);

	private:
		DECLARE_EVENT_TABLE();

		wxTextCtrl *EditCheatName;
		wxSpinButton *EntrySelection;
		wxTextCtrl *EditCheatCode;

		enum {
			ID_EDITCHEAT_NAME_TEXT = 4550,
			ID_EDITCHEAT_NAME,
			ID_ENTRY_SELECT,
			ID_EDITCHEAT_CODE
		};

		void SaveCheatData(wxCommandEvent& event);
		void ChangeEntry(wxSpinEvent& event);
		void UpdateTextCtrl(ActionReplay::ARCode arCode);

		int selection;

};
#endif // __PATCH_ADDEDIT_h__
