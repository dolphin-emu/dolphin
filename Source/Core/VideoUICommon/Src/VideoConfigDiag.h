
#ifndef _CONFIG_DIAG_H_
#define _CONFIG_DIAG_H_

#include <vector>
#include <string>

#include "VideoConfig.h"

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

	void UpdateValue(wxCommandEvent& ev)
	{
		m_setting = (ev.GetInt() != 0) ^ m_reverse;
	}
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
	VideoConfigDiag(wxWindow* parent, const std::string &title,
		const std::vector<std::string> &adapters = std::vector<std::string>(),
		const std::vector<std::string> &aamodes = std::vector<std::string>(),
		const std::vector<std::string> &ppshader = std::vector<std::string>());

	VideoConfig &vconfig;

protected:
	void Event_StcSafe(wxCommandEvent &) { vconfig.iSafeTextureCache_ColorSamples = 0; }
	void Event_StcNormal(wxCommandEvent &) { vconfig.iSafeTextureCache_ColorSamples = 512; }
	void Event_StcFast(wxCommandEvent &) { vconfig.iSafeTextureCache_ColorSamples = 128; }

	void Event_PPShader(wxCommandEvent &ev)
	{
		const int sel = ev.GetInt();
		if (sel)
			vconfig.sPostProcessingShader = ev.GetString().mb_str();
		else
			vconfig.sPostProcessingShader.clear();
	}

	void CloseDiag(wxCommandEvent&);
};

#endif
