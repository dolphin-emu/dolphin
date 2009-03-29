// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef __DSP_LLE_CONFIGDIALOG_h__
#define __DSP_LLE_CONFIGDIALOG_h__

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/button.h>
#include <wx/statbox.h>

class DSPConfigDialogLLE : public wxDialog
{
public:
	DSPConfigDialogLLE(wxWindow *parent,
		     wxWindowID id = 1,
		     const wxString &title = wxT("Dolphin DSP-LLE Plugin Settings"),
		     const wxPoint& pos = wxDefaultPosition,
		     const wxSize& size = wxDefaultSize,
		     long style = wxDEFAULT_DIALOG_STYLE);
    virtual ~DSPConfigDialogLLE();
    void AddBackend(const char *backend);

private:
    DECLARE_EVENT_TABLE();
    
    wxButton *m_OK;
    wxCheckBox *m_buttonEnableHLEAudio;
    wxCheckBox *m_buttonEnableDTKMusic;
    wxCheckBox *m_buttonEnableThrottle;
    wxArrayString wxArrayBackends;
    wxComboBox  *m_BackendSelection;

    enum
	{
	    wxID_OK,
	    ID_ENABLE_HLE_AUDIO,
	    ID_ENABLE_DTK_MUSIC,
	    ID_ENABLE_THROTTLE,
	    ID_BACKEND
	};
    
    void OnOK(wxCommandEvent& event);
    void SettingsChanged(wxCommandEvent& event);
};

#endif //__DSP_LLE_CONFIGDIALOG_h__
