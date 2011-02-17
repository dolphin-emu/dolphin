
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

	void Event_OnProfileChange(wxCommandEvent& ev);

	// Enables/disables UI elements depending on current config - if appropriate also updates g_Config
	void OnUpdateUI(wxUpdateUIEvent& ev);

	// Refresh UI values from current config (used when reloading config)
	void SetUIValuesFromConfig();

	// Don't mess with keeping two comboboxes in sync, use only one CB instead..
	SettingChoice* profile_cb; // "General" tab
	wxStaticText* profile_text; // "Advanced" tab

	wxChoice* choice_adapter;
	wxChoice* choice_aspect;
	SettingCheckBox* widescreen_hack;
	SettingCheckBox* vsync;

	SettingChoice* anisotropic_filtering;
	wxStaticText* text_aamode;
	SettingChoice* choice_aamode;

	SettingCheckBox* native_mips;
	SettingCheckBox* efb_scaled_copy;
	SettingCheckBox* pixel_lighting;
	SettingCheckBox* pixel_depth;
	SettingCheckBox* force_filtering;
	SettingCheckBox* _3d_vision;

	wxChoice* choice_efbscale;
	SettingCheckBox* efbaccess_enable;
	SettingCheckBox* emulate_efb_format_changes;

	SettingCheckBox* efbcopy_enable;
	SettingRadioButton* efbcopy_texture;
	SettingRadioButton* efbcopy_ram;
	SettingCheckBox* cache_efb_copies;

	SettingCheckBox* stc_enable;
	wxRadioButton* stc_safe;
	wxRadioButton* stc_normal;
	wxRadioButton* stc_fast;

	SettingCheckBox* wireframe;
	SettingCheckBox* disable_lighting;
	SettingCheckBox* disable_textures;
	SettingCheckBox* disable_fog;
	SettingCheckBox* disable_dst_alpha;

	SettingCheckBox* show_fps;
	SettingCheckBox* overlay_stats;
	SettingCheckBox* overlay_proj_stats;
	SettingCheckBox* texfmt_overlay;
	SettingCheckBox* efb_copy_regions;
	SettingCheckBox* show_shader_errors;
	SettingCheckBox* show_input_display;

	SettingCheckBox* enable_xfb;
	SettingRadioButton* virtual_xfb;
	SettingRadioButton* real_xfb;

	SettingCheckBox* dump_textures;
	SettingCheckBox* hires_textures;
	SettingCheckBox* dump_efb;
	SettingCheckBox* dump_frames;
	SettingCheckBox* free_look;
	SettingCheckBox* frame_dumps_via_ffv1;

	SettingCheckBox* crop;
	SettingCheckBox* opencl;
	SettingCheckBox* dlcache;
	SettingCheckBox* hotkeys;

	wxChoice* choice_ppshader;

	// TODO: Add options for
	//vconfig.bTexFmtOverlayCenter
	//vconfig.bAnaglyphStereo
	//vconfig.iAnaglyphStereoSeparation
	//vconfig.iAnaglyphFocalAngle
	//vconfig.bShowEFBCopyRegions
	//vconfig.iCompileDLsLevel

	VideoConfig vconfig;
	std::string ininame;
	int cur_profile;
};

#endif
