#pragma once

#ifndef __CDebugger_h__
#define __CDebugger_h__

// general things
#include <iostream>
#include <vector>

#ifndef WX_PRECOMP
#include <wx/wx.h>
#include <wx/dialog.h>
#else
#include <wx/wxprec.h>
#endif

#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/statbmp.h>
#include <wx/datetime.h> // for the timestamps

#include <wx/sizer.h>
#include <wx/notebook.h>
#include <wx/filepicker.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>

//#include "../Globals.h"

//class CPBView;
//class IniFile;

// Window settings
#undef CDebugger_STYLE
#define CDebugger_STYLE wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN | wxNO_FULL_REPAINT_ON_RESIZE

class CDebugger : public wxDialog
{
private:
	DECLARE_EVENT_TABLE();

public:
	CDebugger(wxWindow *parent, wxWindowID id = 1, const wxString &title = _("Sound Debugger"),
		const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
		long style = CDebugger_STYLE);

	virtual ~CDebugger();

	void DoHide();
	void DoShow();
private:
	// WARNING: Make sure these are not also elsewhere
	enum
	{
		// = 2000,
	};

	void OnClose(wxCloseEvent& event);		
	void CreateGUIControls();		
};

#endif
