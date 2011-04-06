
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
#include <wx/fontmap.h>

enum tmp_MemberClass {
	Def_Data,
	State
};

enum tmp_TypeClass {
	only_2State,
	allow_3State
};

struct _pattern
{
protected:
	_pattern(bool *state, const tmp_TypeClass type);
	
	bool *m_uistate;
	const tmp_TypeClass _type;

public:
	/*
		Couldn't make this a pure abstract class since wxWidgets in use(v.2.8.x) is buggy;
		oddly, CommandEventHandler fails to connect&retrieve the correct target function.
		Newer wxWidgets versions don't have this problem...
	*/
	//virtual void UpdateValue(wxCommandEvent& ev) = 0;
	//virtual void UpdateUIState(bool state) = 0;
	//virtual void ChangeRefDataMember(tmp_MemberClass type, void *newRef) = 0;

	tmp_TypeClass getTypeClass()
	{
		// This method returns what kind of support has the Object (2State or 3State).
		// NOTE: this doesn't return a run-time Control-state, since a 3State Control
		// can to switch to 2State and vice-versa, while an Object 2State only can't.
		return _type;
	}
};

class SettingCheckBox : public _pattern, public wxCheckBox
{
	DECLARE_CLASS(SettingCheckBox)

public:
	SettingCheckBox(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse = false, long style = 0);
	
	// overload constructor
	SettingCheckBox(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool &def_setting, bool &state, bool reverse = false, long style = 0);

	void UpdateValue(wxCommandEvent& ev)
	{
		if (_type == only_2State)
		{
			m_setting = (ev.GetInt() != 0) ^ m_reverse;
		}
		else
		{
			bool style = (this->GetWindowStyle() == (wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER));

			if (style)
			{
				// changing state value should be done here, never outside this block
				UpdateUIState(ev.GetInt() != wxCHK_UNDETERMINED);
			}

			m_setting = (ev.GetInt() == wxCHK_CHECKED);
			if (m_uistate)
			{
				// if state = false and Checkbox ctrl allow 3RD STATE, then data = default_data
				m_setting = (style && !*m_uistate) ? *d_setting : m_setting;
			}

			m_setting = m_setting ^ m_reverse;

			if (!style && m_uistate) // this guarantees bidirectional access to default value
				if (!*m_uistate) *d_setting = m_setting;
		}
		ev.Skip();
	}

	void UpdateUIState(bool state)
	{
		if (m_uistate && this->GetWindowStyle() == (wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER))
		{
			*m_uistate = state;
			if (!*m_uistate)
				m_setting = *d_setting ^ m_reverse;
		}
	}

	void ChangeRefDataMember(tmp_MemberClass type, void *newRef)
	{
		switch (type)
		{
			case Def_Data:
				d_setting = (bool*)newRef;
				break;
			case State:
				m_uistate = (bool*)newRef;
				break;
		}
	}

private:
	bool &m_setting;
	bool *d_setting;
	const bool m_reverse;
};

template <typename V>
class SettingChoice : public _pattern, public wxChoice
{
	DECLARE_CLASS(SettingChoice)

public:
	SettingChoice(wxWindow* parent, V &setting, const wxString& tooltip, int num = 0, const wxString choices[] = NULL, long style = 0);
	
	// overload constructor
	SettingChoice(wxWindow* parent, V &setting, V &def_setting, bool &state, int &cur_index, const wxString& tooltip, int num = 0, const wxString choices[] = NULL, long style = 0);
	
	void UpdateValue(wxCommandEvent& ev)
	{
		m_setting = ev.GetInt();
		if (_type == allow_3State)
		{
			if (m_index != 0) // Choice ctrl with 3RD option
			{
				// changing state value should be done here, never outside this block
				if (m_setting == 0)
				{
					UpdateUIState(false);
				}
				else
				{
					UpdateUIState(true);
					m_setting -= 1;
				}
			}
			else // Choice ctrl without 3RD option
			{
				if (m_uistate)
					if (!*m_uistate) *d_setting = m_setting;
			}
		}
		ev.Skip();
	}

	void UpdateUIState(bool state)
	{
		if (m_uistate && m_index != 0)
		{
			*m_uistate = state;
			if (!*m_uistate)
				m_setting = *d_setting;
		}
	}

	void ChangeRefDataMember(tmp_MemberClass type, void *newRef)
	{
		switch (type)
		{
			case Def_Data:
				d_setting = (V*)newRef;
				break;
			case State:
				m_uistate = (bool*)newRef;
				break;
		}
	}

private:
	V &m_setting;
	V *d_setting;
	int &m_index;
};

typedef SettingChoice<int> IntSettingChoice;
typedef SettingChoice<std::string> StringSettingChoice;

class SettingRadioButton : public _pattern, public wxRadioButton
{
	DECLARE_CLASS(SettingRadioButton)

public:
	SettingRadioButton(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse = false, long style = 0);
	
	void UpdateValue(wxCommandEvent& ev)
	{
		m_setting = (ev.GetInt() != 0) ^ m_reverse;
		ev.Skip();
	}

	void UpdateUIState(bool state) {}
	void ChangeRefDataMember(tmp_MemberClass type, void *newRef) {}


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

class CGameListCtrl;

class VideoConfigDiag : public wxDialog
{
public:
	VideoConfigDiag(wxWindow* parent, const std::string &title, const std::string& ininame);

protected:
	void Event_Backend(wxCommandEvent &ev) { ev.Skip(); } // TODO: Query list of supported AA modes
	void Event_Adapter(wxCommandEvent &ev) { ev.Skip(); } // TODO

	void Event_StcSafe(wxCommandEvent &ev) { cur_vconfig.iSafeTextureCache_ColorSamples = 0; ev.Skip(); }
	void Event_StcNormal(wxCommandEvent &ev) { cur_vconfig.iSafeTextureCache_ColorSamples = 512; ev.Skip(); }
	void Event_StcFast(wxCommandEvent &ev) { cur_vconfig.iSafeTextureCache_ColorSamples = 128; ev.Skip(); }

	void Event_ClickClose(wxCommandEvent&);
	void Event_ClickDefault(wxCommandEvent&);
	void Event_Close(wxCloseEvent&);

	void Event_OnProfileChange(wxCommandEvent& ev);

	// Enables/disables UI elements depending on current config - if appropriate also updates g_Config
	void OnUpdateUI(wxUpdateUIEvent& ev);

	// Refresh UI values from current config (used when reloading config)
	void SetUIValuesFromConfig();

	// Redraw the aspect about some UI controls when a profile is selected
	void ChangeStyle();

	// Don't mess with keeping two comboboxes in sync, use only one CB instead..
	SettingChoice<int>* profile_cb; // "General" tab
	wxStaticText* profile_text; // "Advanced" tab

	IntSettingChoice* choice_adapter;
	IntSettingChoice* choice_aspect;
	SettingCheckBox* widescreen_hack;
	SettingCheckBox* vsync;

	IntSettingChoice* anisotropic_filtering;
	wxStaticText* text_aamode;
	IntSettingChoice* choice_aamode;

	SettingCheckBox* native_mips;
	SettingCheckBox* efb_scaled_copy;
	SettingCheckBox* pixel_lighting;
	SettingCheckBox* pixel_depth;
	SettingCheckBox* force_filtering;
	SettingCheckBox* _3d_vision;

	IntSettingChoice* choice_efbscale;
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
	SettingCheckBox* ompdecoder;
	StringSettingChoice* choice_ppshader;

	wxButton* btn_default;
	// TODO: Add options for
	//cur_vconfig.bTexFmtOverlayCenter
	//cur_vconfig.bAnaglyphStereo
	//cur_vconfig.iAnaglyphStereoSeparation
	//cur_vconfig.iAnaglyphFocalAngle
	//cur_vconfig.bShowEFBCopyRegions
	//cur_vconfig.iCompileDLsLevel
	wxPoint CenterCoords;
	VideoConfig cur_vconfig;
	VideoConfig def_vconfig;
	std::string ininame;
	int cur_profile;
	int prev_profile;
	const CGameListCtrl *GameListCtrl;
};

#endif
