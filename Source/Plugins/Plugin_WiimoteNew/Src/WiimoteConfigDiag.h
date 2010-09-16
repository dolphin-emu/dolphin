
#ifndef WIIMOTE_CONFIG_DIAG_H
#define WIIMOTE_CONFIG_DIAG_H

#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>

#include "ConfigDiag.h"

class WiimoteConfigPage : public wxNotebookPage
{
public:
	WiimoteConfigPage(wxWindow* const parent, const int index);

#ifdef _WIN32
	void PairUpRealWiimotes(wxCommandEvent& event);
#endif
	void RefreshRealWiimotes(wxCommandEvent& event);

	void SelectSource(wxCommandEvent& event);

private:
	const int	m_index;
	
	wxStaticText* m_connected_wiimotes_txt;
	wxChoice* m_input_src_choice;
};

class WiimoteConfigDiag : public wxDialog
{
public:
	WiimoteConfigDiag(wxWindow* const parent, InputPlugin& plugin);
	
	void ConfigEmulatedWiimote(wxCommandEvent& event);
	void Save(wxCommandEvent& event);

private:
	InputPlugin&	m_plugin;

	wxNotebook*		m_pad_notebook;

};


#endif
