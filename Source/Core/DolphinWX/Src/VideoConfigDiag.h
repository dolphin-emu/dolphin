
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

class SettingCheckBox : public wxCheckBox
{
public:
	SettingCheckBox(wxWindow* parent, const wxString& label, const wxString& tooltip,
		bool &setting, long style = 0);

	void UpdateValue(wxCommandEvent& ev)
	{
		int const val = Get3StateValue();
		*reinterpret_cast<u8*>(&m_setting) = (u8)val;
		ev.Skip();
	}

private:
	bool &m_setting;
};

class SettingRadioButton : public wxRadioButton
{
public:
	SettingRadioButton(wxWindow* parent, const wxString& label, const wxString& tooltip,
		bool &setting, bool reverse = false, long style = 0);

	void UpdateValue(wxCommandEvent& ev)
	{
		m_setting = (ev.GetInt() != 0) ^ m_reverse;
		ev.Skip();
	}

private:
	bool &m_setting;
	const bool m_reverse;
};

template <typename T>
class IntegerSetting : public wxSpinCtrl
{
public:
	IntegerSetting(wxWindow* parent, const wxString& label, T& setting, int minVal, int maxVal, long style = 0);

	void UpdateValue(wxCommandEvent& ev)
	{
		m_setting = ev.GetInt();
		ev.Skip();
	}

private:
	T& m_setting;
};

typedef IntegerSetting<u32> U32Setting;

class SettingChoice : public wxChoice
{
public:
	SettingChoice(wxWindow* parent, int &setting, const wxString& tooltip, int num = 0, const wxString choices[] = NULL, long style = 0);
	
	void UpdateValue(wxCommandEvent& ev);

private:
	int &m_setting;
};

class CGameListCtrl;

class VideoConfigDiag : public wxDialog
{
public:
	VideoConfigDiag(wxWindow* parent, const std::string &title, bool is_game_config = false);

private:
	void Event_Backend(wxCommandEvent &ev) { ev.Skip(); } // TODO: Query list of supported AA modes
	void Event_Adapter(wxCommandEvent &ev) { ev.Skip(); } // TODO

	void Event_StcSafe(wxCommandEvent &ev) { vconfig.iSafeTextureCache_ColorSamples = 0; ev.Skip(); }
	void Event_StcNormal(wxCommandEvent &ev) { vconfig.iSafeTextureCache_ColorSamples = 512; ev.Skip(); }
	void Event_StcFast(wxCommandEvent &ev) { vconfig.iSafeTextureCache_ColorSamples = 128; ev.Skip(); }

	void Event_PPShader(wxCommandEvent &ev)
	{
		const int sel = ev.GetInt();
		if (sel)
			vconfig.sPostProcessingShader = ev.GetString().mb_str();
		else
			vconfig.sPostProcessingShader.clear();
		ev.Skip();
	}

	void Event_ClickClose(wxCommandEvent&);
	void Event_Close(wxCloseEvent&);

	// Refresh UI values from current config (used when reloading config)
	void SetUIValuesFromConfig();

	VideoConfig& vconfig;
};

#endif
