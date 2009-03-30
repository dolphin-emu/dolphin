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

#include "Common.h" // Common
#include <iostream> // System
#include <fstream>
#include <sstream>

#include "Debugger.h"

// Event table and class
BEGIN_EVENT_TABLE(CDebugger,wxDialog)	
	EVT_CLOSE(CDebugger::OnClose) // on close event

	//EVT_RIGHT_DOWN(CDebugger::ScrollBlocks)
	//EVT_LEFT_DOWN(CDebugger::ScrollBlocks)
	//EVT_MOUSE_EVENTS(CDebugger::ScrollBlocks)
	//EVT_MOTION(CDebugger::ScrollBlocks)
	//EVT_SCROLL(CDebugger::ScrollBlocks)
	//EVT_SCROLLWIN(CDebugger::ScrollBlocks)
END_EVENT_TABLE()

CDebugger::CDebugger(wxWindow *parent, wxWindowID id, const wxString &title,
					 const wxPoint &position, const wxSize& size, long style)
					 : wxDialog(parent, id, title, position, size, style)
{
	CreateGUIControls();
}

CDebugger::~CDebugger()
{
} 

void CDebugger::OnClose(wxCloseEvent& /*event*/)
{	
	EndModal(0);
}

void CDebugger::DoHide()
{
	Hide();
}

void CDebugger::DoShow()
{
	Show();
	//DoShowConsole(); // The console goes with the wx window
}

void CDebugger::CreateGUIControls()
{
	SetTitle(wxT("Sound Debugging"));

	// Basic settings
	SetIcon(wxNullIcon);
	SetSize(8, 8, 200, 100); // These will become the minimin sizes allowed by resizing
	Center();

}
