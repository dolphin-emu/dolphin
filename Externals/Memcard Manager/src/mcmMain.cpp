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

void __Log(int logNumber, const char* text, ...){logNumber; text;}
void __Logv(int log, int v, const char *format, ...){log; v; format;}

#include "MemcardManager.h"
class MCMApp
	: public wxApp
{
	public:
		bool OnInit();
		
};

IMPLEMENT_APP(MCMApp)

CMemcardManager *main_frame;

#if defined HAVE_WX && HAVE_WX 
bool wxMsgAlert(const char* caption, const char* text, bool yes_no, int Style) 
{
	return wxYES == wxMessageBox(wxString::FromAscii(text), 
				 wxString::FromAscii(caption),
				 (yes_no)?wxYES_NO:wxOK);
}
#endif

bool MCMApp::OnInit()
{
	// Register message box handler
	#if defined(HAVE_WX) && HAVE_WX
		RegisterMsgAlertHandler(&wxMsgAlert);
	#endif

	main_frame = new CMemcardManager((wxFrame*) NULL, wxID_ANY, wxString::FromAscii("Memcard Manager"),
				wxPoint(100, 100), wxSize(800, 600));
	main_frame->Show();
	SetTopWindow(main_frame);
	return true;
}

