//////////////////////////////////////////////////////////////////////////////////////////
//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//
//////////////////////////////////////////////////////////////////////////////////////////

#ifndef __CDebugger_h__
#define __CDebugger_h__


#include "../Globals.h"

class CPBView;
class IniFile;

// =======================================================================================
// Window settings - I'm not sure what these do. I just copied them gtom elsewhere basically.
#undef CDebugger_STYLE

#define CDebugger_STYLE wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN | wxNO_FULL_REPAINT_ON_RESIZE
// =======================================================================================

class CDebugger : public wxDialog
{
	private:
		DECLARE_EVENT_TABLE();
		
	public:
		CDebugger(wxWindow *parent, wxWindowID id = 1, const wxString &title = wxT("Sound Debugger"),
			const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
			long style = CDebugger_STYLE);

		virtual ~CDebugger();

		void Save(IniFile& _IniFile) const;
		void Load(IniFile& _IniFile);
	
		void NotifyUpdate();
		void OnUpdate(wxCommandEvent& event);

		CPBView* m_GPRListView;
		

	private:
		// ---------------------------------------------------------------------------------------
		// WARNING: Make sure these are not also elsewhere, for example in resource.h.
		enum
		{
			IDC_CHECK0 = 2000,
			IDC_CHECK1,
			IDC_RADIO0,
			IDC_RADIO1,
			IDC_RADIO2,
			IDC_RADIO3,
			IDC_RADIO4,
			ID_UPD,
			ID_SELC,
			ID_PRESETS,
			ID_GPR,
			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values

		};
		// ---------------------------------------------------------------------------------------
		
		
		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();		
};

#endif
