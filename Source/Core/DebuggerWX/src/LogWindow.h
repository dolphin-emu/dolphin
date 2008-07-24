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

#ifndef LOGWINDOW_H_
#define LOGWINDOW_H_

class wxTextCtrl;
class wxCheckListBox;
class IniFile;

class CLogWindow
	: public wxDialog
{
	public:

		CLogWindow(wxWindow* parent);
		void NotifyUpdate();

		void Save(IniFile& _IniFile) const;
		void Load(IniFile& _IniFile);

	private:

		enum { LogBufferSize = 8 * 1024 * 1024};
		char m_logBuffer[LogBufferSize];
		wxTextCtrl* m_log, * m_cmdline;
		wxCheckListBox* m_checks;
		bool m_bCheckDirty;
		DECLARE_EVENT_TABLE()

		void OnSubmit(wxCommandEvent& event);
		void OnUpdateLog(wxCommandEvent& event);
		void OnLogCheck(wxCommandEvent& event);
		void OnClear(wxCommandEvent& event);

		void UpdateChecks();
		void UpdateLog();
};

#endif /*LOGWINDOW_H_*/
