
#ifndef _CONFIG_DIAG_H_
#define _CONFIG_DIAG_H_

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>

template <typename W>
class BoolSetting : public W
{
public:
	BoolSetting(wxWindow* parent, const wxString& label, bool &setting, bool reverse = false, long style = 0);
	void UpdateValue(wxCommandEvent& ev);
private:
	bool &m_setting;
	const bool m_reverse;
};

class SettingChoice : public wxChoice
{
public:
	SettingChoice(wxWindow* parent, int &setting, int num = 0, const wxString choices[] = NULL);
	void UpdateValue(wxCommandEvent& ev);
private:
	int &m_setting;
};

class VideoConfigDiag : public wxDialog
{
public:
	VideoConfigDiag(wxWindow* parent);

protected:
	void CloseDiag(wxCommandEvent&);
};

#endif
