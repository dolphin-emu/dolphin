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


//////////////////////////////////////////////////////////////////////////////////////////
// Includes
// -------------
#include <iostream>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <stdlib.h>
#endif

#include "ConsoleWindow.h" // Open and close console

#include "Debugger.h"
#include "PBView.h"
#include "IniFile.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "FileSearch.h"
//#include "../Logging/File.h" // Write to file
///////////////////////////////


// Make the wxTextCtrls scroll with each other
void CDebugger::DoScrollBlocks()
{
	// ShowPosition = in letters
	// GetScrollPos = number of lines from the top
	// GetLineLength = letters in one line
	// SetScrollPos = only set the scrollbar, doesn't update the text,
		// Update() or Refresh() doesn't help

	double pos = m_bl95->GetScrollPos(wxVERTICAL)*(m_bl95->GetLineLength(0)+12.95); // annoying :(
	m_bl0->ShowPosition((int)pos);

	/*
	if(GetAsyncKeyState(VK_NUMPAD1))
		A -= 0.1;
	else if(GetAsyncKeyState(VK_NUMPAD2))
		A += 0.11;

	Console::Print("GetScrollPos:%i GetScrollRange:%i GetPosition:%i GetLastPosition:%i GetMaxWidth:%i \
			GetLineLength:%i XYToPosition:%i\n \
			GetScrollPos * GetLineLength + GetScrollRange:%i A:%f\n",
		m_bl95->GetScrollPos(wxVERTICAL), m_bl95->GetScrollRange(wxVERTICAL),
		m_bl95->GetPosition().y, m_bl95->GetLastPosition(), m_bl95->GetMaxWidth(),
		m_bl95->GetLineLength(0), m_bl95->XYToPosition(0,25),
		pos, A
		);

	for (int i = 0; i < 127; ++i)
	{
		m_bl0->AppendText(wxString::Format("%02i|68 : 01a70144\n", i));
		m_bl95->AppendText(wxString::Format("%i Mouse\n", i));
	}*/
}

void CDebugger::ScrollBlocksMouse(wxMouseEvent& event)
{
	DoScrollBlocks();
	event.Skip(); // otherwise we remove the regular behavior, for example scrolling
}

void CDebugger::ScrollBlocksCursor(wxScrollWinEvent& event)
{
	DoScrollBlocks();
	event.Skip(); // otherwise we remove the regular behavior, for example scrolling
}
// ==============
