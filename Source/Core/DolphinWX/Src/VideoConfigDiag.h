
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
	BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse = false, long style = 0);

	void UpdateValue(wxCommandEvent& ev)
	{
		m_setting = (ev.GetInt() != 0) ^ m_reverse;
		ev.Skip();
	}
private:
	bool &m_setting;
	const bool m_reverse;
};

typedef BoolSetting<wxCheckBox> SettingCheckBox;
typedef BoolSetting<wxRadioButton> SettingRadioButton;

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

class VideoConfigDiag : public wxDialog
{
public:
	VideoConfigDiag(wxWindow* parent, const std::string &title, const std::string& ininame);

protected:
	void Event_Backend(wxCommandEvent &ev) { ev.Skip(); } // TODO: Query list of supported AA modes
	void Event_Adapter(wxCommandEvent &ev) { ev.Skip(); } // TODO

	void Event_Stc(wxCommandEvent &ev)
	{
		int samples[] = { 0, 512, 128 };
		if (ev.GetInt() < 3)
		{
			vconfig.iSafeTextureCache_ColorSamples = samples[ev.GetInt()];
			vconfig.bSafeTextureCache = true;
		}
		else vconfig.bSafeTextureCache = false;

		ev.Skip();
	}

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

	// Enables/disables UI elements depending on current config
	void OnUpdateUI(wxUpdateUIEvent& ev)
	{
		// Anti-aliasing
		choice_aamode->Enable(vconfig.backend_info.AAModes.size() > 1);
		text_aamode->Enable(vconfig.backend_info.AAModes.size() > 1);

		// pixel lighting
		pixel_lighting->Enable(vconfig.backend_info.bSupportsPixelLighting);

		// 3D vision
		_3d_vision->Show(vconfig.backend_info.bSupports3DVision);

		// EFB copy
		efbcopy_texture->Enable(vconfig.bEFBCopyEnable);
		efbcopy_ram->Enable(vconfig.bEFBCopyEnable);
		cache_efb_copies->Enable(vconfig.bEFBCopyEnable && !vconfig.bCopyEFBToTexture);

		// EFB format change emulation
		emulate_efb_format_changes->Enable(vconfig.backend_info.bSupportsFormatReinterpretation);

		// XFB
		virtual_xfb->Enable(vconfig.bUseXFB);
		real_xfb->Enable(vconfig.bUseXFB);

		ev.Skip();
	}

	wxStaticText* text_aamode;
	SettingChoice* choice_aamode;

	SettingCheckBox* pixel_lighting;

	SettingCheckBox* _3d_vision;

	SettingRadioButton* efbcopy_texture;
	SettingRadioButton* efbcopy_ram;
	SettingCheckBox* cache_efb_copies;
	SettingCheckBox* emulate_efb_format_changes;

	SettingRadioButton* virtual_xfb;
	SettingRadioButton* real_xfb;

	VideoConfig &vconfig;
	std::string ininame;
};

#endif
