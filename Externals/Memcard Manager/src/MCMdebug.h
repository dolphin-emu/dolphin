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

#ifndef __MCM_H__
#define __MCM_H__

#include "MemcardManager.h"
#include <wx/gbsizer.h>
#include <wx/notebook.h>


class CMemcardManagerDebug : public wxFrame
{
	public:
		
		CMemcardManagerDebug(wxFrame* parent, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize);
		virtual ~CMemcardManagerDebug(){};
		void updatePanels(GCMemcard ** memCard,int card);
		
	private:
		enum
		{
			ID_NOTEBOOK_MAIN,
			ID_TAB_HDR,
			ID_TAB_DIR,
			ID_TAB_DIR_B,
			ID_TAB_BAT,
			ID_TAB_BAT_B,
		};

		

		GCMemcard **memoryCard;

		wxNotebook *m_Notebook_MCMD;
		
		wxPanel *m_Tab_HDR,
				*m_Tab_DIR,
				*m_Tab_DIR_b,
				*m_Tab_BAT,
				*m_Tab_BAT_b;

		wxBoxSizer *sMain,
			*sDebug,
			*sDebug2;
		
		wxStaticText	*t_HDR_ser[2],
						*t_HDR_fmtTime[2],
						*t_HDR_SRAMBIAS[2],
						*t_HDR_SRAMLANG[2],
						*t_HDR_Unk2[2],
						*t_HDR_devID[2],
						*t_HDR_Size[2],
						*t_HDR_Encoding[2],
						*t_HDR_UpdateCounter[2],
						*t_HDR_CheckSum1[2],
						*t_HDR_CheckSum2[2],

						*t_DIR_UpdateCounter[2],
						*t_DIR_CheckSum1[2],
						*t_DIR_CheckSum2[2],
						
						*t_DIR_b_UpdateCounter[2],
						*t_DIR_b_CheckSum1[2],
						*t_DIR_b_CheckSum2[2],
						
						*t_BAT_CheckSum1[2],
						*t_BAT_CheckSum2[2],
						*t_BAT_UpdateCounter[2],
						*t_BAT_FreeBlocks[2],
						*t_BAT_LastAllocated[2],
						*t_BAT_map[256][2],
						
						*t_BAT_b_CheckSum1[2],
						*t_BAT_b_CheckSum2[2],
						*t_BAT_b_UpdateCounter[2],
						*t_BAT_b_FreeBlocks[2],
						*t_BAT_b_LastAllocated[2];

		DECLARE_EVENT_TABLE();
		void Init_ChildControls();
		void OnClose(wxCloseEvent& event);

		void Init_HDR();
		void Init_DIR();
		void Init_DIR_b();
		void Init_BAT();
		void Init_BAT_b();
		void updateHDRtab(int card);
		void updateDIRtab(int card);
		void updateDIRBtab(int card);
		void updateBATtab(int card);
		void updateBATBtab(int card);

};

#endif
