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

#ifndef __DSP_HLE_CONFIGDIALOG_h__
#define __DSP_HLE_CONFIGDIALOG_h__

#include <wx/wx.h>
#include <wx/dialog.h>
#include <wx/button.h>
#include <wx/statbox.h>
#include "AudioCommon.h"

class DSPConfigDialogHLE : public wxDialog
{
public:
	DSPConfigDialogHLE(wxWindow *parent,
		     wxWindowID id = 1,
		     const wxString &title = wxT("Dolphin DSP-HLE Plugin Settings"),
		     const wxPoint& pos = wxDefaultPosition,
		     const wxSize& size = wxDefaultSize,
		     long style = wxDEFAULT_DIALOG_STYLE);
    virtual ~DSPConfigDialogHLE();
    void AddBackend(const char *backend);
	void ClearBackends();
    
private:
    DECLARE_EVENT_TABLE();
    
	wxSlider *m_volumeSlider;
	wxStaticText *m_volumeText;
    wxButton *m_OK;
    wxCheckBox *m_buttonEnableHLEAudio;
    wxCheckBox *m_buttonEnableDTKMusic;
    wxCheckBox *m_buttonEnableThrottle;
	wxCheckBox *m_buttonEnableRE0Fix;
    wxArrayString wxArrayBackends;
    wxComboBox  *m_BackendSelection;

    enum
	{
	    wxID_OK,
	    ID_ENABLE_HLE_AUDIO,
	    ID_ENABLE_DTK_MUSIC,
	    ID_ENABLE_THROTTLE,
		ID_ENABLE_RE0_FIX,
	    ID_BACKEND,
		ID_VOLUME
	};
    
    void OnOK(wxCommandEvent& event);
    void SettingsChanged(wxCommandEvent& event);
	void VolumeChanged(wxScrollEvent& event);
};

#endif //__DSP_HLE_CONFIGDIALOG_h__
