#include "VideoConfigDiag.h"

#include "FileUtil.h"
#include "TextureCacheBase.h"

#include <wx/intl.h>

#include "Frame.h"
#include "ISOFile.h"
#include "GameListCtrl.h"

#include "ConfigManager.h"
#include "Core.h"
#include "Host.h"

extern CFrame* main_frame;

#define _connect_macro_(b, f, c, s)	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler( f ), (wxObject*)0, (wxEvtHandler*)s)

_pattern::_pattern(bool *state, const tmp_TypeClass type): m_uistate(state), _type(type) {}

IMPLEMENT_CLASS(SettingCheckBox, wxCheckBox) // this allows to register the class in RTTI passing through wxWidgets
SettingCheckBox::SettingCheckBox(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool &def_setting, bool &state, bool reverse, long style)
	: _pattern(&state, allow_3State)
	, wxCheckBox(parent, -1, label, wxDefaultPosition, wxDefaultSize, style)
	, m_setting(setting)
	, d_setting(&def_setting)
	, m_reverse(reverse)
{
	SetToolTip(tooltip);
	SetValue(m_setting ^ m_reverse);
	_connect_macro_(this, SettingCheckBox::UpdateValue, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
}

// without 3sate support
SettingCheckBox::SettingCheckBox(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse, long style)
	: _pattern(0, only_2State)
	, wxCheckBox(parent, -1, label, wxDefaultPosition, wxDefaultSize, style)
	, m_setting(setting)
	, d_setting(&setting)
	, m_reverse(reverse)
{
	SetToolTip(tooltip);
	SetValue(m_setting ^ m_reverse);
	_connect_macro_(this, SettingCheckBox::UpdateValue, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
}


IMPLEMENT_CLASS(SettingRadioButton, wxRadioButton)
SettingRadioButton::SettingRadioButton(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse, long style)
	: _pattern(0, only_2State)
	, wxRadioButton(parent, -1, label, wxDefaultPosition, wxDefaultSize, style)
	, m_setting(setting)
	, m_reverse(reverse)
{
	SetToolTip(tooltip);
	SetValue(m_setting ^ m_reverse);
	_connect_macro_(this, SettingRadioButton::UpdateValue, wxEVT_COMMAND_RADIOBUTTON_SELECTED, this);
}

// partial specialization (SettingChoice template)
template<>
void StringSettingChoice::UpdateValue(wxCommandEvent& ev)
{
	m_setting = ev.GetString().mb_str();
	if (_type == allow_3State)
	{
		if (m_index != 0) // Choice ctrl with 3RD option
		{
			// changing state value should be done here, never outside this block
			if (ev.GetInt() == 0)
			{
				UpdateUIState(false);
			}
			else
			{
				UpdateUIState(true);
				if (ev.GetInt() == 1) m_setting.clear();
			}
		}
		else // Choice ctrl without 3RD option
		{
			if (ev.GetInt() == 0)
				m_setting.clear();
			if (m_uistate)
				if (!*m_uistate)
					*d_setting = m_setting;
		}
	}
	ev.Skip();
}

template<>
wxClassInfo IntSettingChoice::ms_classInfo(wxT("IntSettingChoice"),
		&wxChoice::ms_classInfo, NULL, (int)sizeof(IntSettingChoice),
		(wxObjectConstructorFn) NULL); 

template<>
wxClassInfo *IntSettingChoice::GetClassInfo() const
{
	return &IntSettingChoice::ms_classInfo;
}

template<>
wxClassInfo StringSettingChoice::ms_classInfo(wxT("StringSettingChoice"),
		&wxChoice::ms_classInfo, NULL, (int)sizeof(StringSettingChoice),
		(wxObjectConstructorFn) NULL); 

template<>
wxClassInfo *StringSettingChoice::GetClassInfo() const
{
	return &StringSettingChoice::ms_classInfo;
}

template <typename V>
SettingChoice<V>::SettingChoice(wxWindow* parent, V &setting, V &def_setting, bool &state, int &cur_index, const wxString& tooltip, int num, const wxString choices[], long style)
	: _pattern(&state, allow_3State)	
	, wxChoice(parent, -1, wxDefaultPosition, wxDefaultSize, num, choices)
	, m_setting(setting)
	, d_setting(&def_setting)
	, m_index(cur_index)
{
	SetToolTip(tooltip);
	_connect_macro_(this, SettingChoice::UpdateValue, wxEVT_COMMAND_CHOICE_SELECTED, this);
}

// without 3state support
template <typename V>
SettingChoice<V>::SettingChoice(wxWindow* parent, V &setting, const wxString& tooltip, int num, const wxString choices[], long style)
	: _pattern(0, only_2State)	
	, wxChoice(parent, -1, wxDefaultPosition, wxDefaultSize, num, choices)
	, m_setting(setting)
	, d_setting(&setting)
	, m_index(setting)
{
	SetToolTip(tooltip);
	_connect_macro_(this, SettingChoice::UpdateValue, wxEVT_COMMAND_CHOICE_SELECTED, this);
}

static void ScanLayouts(wxWindow *obj)
{
	const wxWindowList container = obj->GetChildren();
	for(wxWindowList::compatibility_iterator node = container.GetFirst(); node; node = node->GetNext())
	{ 
		wxWindow *ctrl = node->GetData();
		if (ctrl->IsKindOf(CLASSINFO(SettingCheckBox)))
		{
			if (((SettingCheckBox*)ctrl)->getTypeClass() == allow_3State)
			{
				((SettingCheckBox*)ctrl)->UpdateUIState(false);
				((SettingCheckBox*)ctrl)->Set3StateValue(wxCHK_UNDETERMINED);
			}
		}
		if (ctrl->IsKindOf(CLASSINFO(IntSettingChoice)))
		{
			if (((IntSettingChoice*)ctrl)->getTypeClass() == allow_3State)
			{
				((IntSettingChoice*)ctrl)->UpdateUIState(false);
				((IntSettingChoice*)ctrl)->Select(0);
			}
		}
		if (ctrl->IsKindOf(CLASSINFO(StringSettingChoice)))
		{
			if (((StringSettingChoice*)ctrl)->getTypeClass() == allow_3State)
			{
				((StringSettingChoice*)ctrl)->UpdateUIState(false);
				((StringSettingChoice*)ctrl)->Select(0);
			}
		}


		if (ctrl->GetChildren().GetCount() > 0)
			ScanLayouts(ctrl); // Exponential Recursive Calls
	}

}

void VideoConfigDiag::Event_ClickDefault(wxCommandEvent&)
{
	ScanLayouts(this); // set UIstate and values
}

void VideoConfigDiag::Event_ClickClose(wxCommandEvent&)
{
	Close();
}

void VideoConfigDiag::Event_Close(wxCloseEvent& ev)
{
	if (cur_profile == 0)
	{
		cur_vconfig.Save((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str());
	}
	else
	{
		const GameListItem* item = GameListCtrl->GetISO(GameListCtrl->GetItemData(cur_profile - 1));
		cur_vconfig.GameIniSave((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str(), 
				(File::GetUserPath(D_GAMECONFIG_IDX) + item->GetUniqueID() + ".ini").c_str());
	}

	EndModal(wxID_OK);

	TextureCache::InvalidateDefer(); // For settings like hi-res textures/texture format/etc.
}


static wxString FormatString(const GameListItem *item)
{
	wxString title;
	if (item->GetCountry() == DiscIO::IVolume::COUNTRY_JAPAN
	|| item->GetCountry() == DiscIO::IVolume::COUNTRY_TAIWAN
	|| item->GetPlatform() == GameListItem::WII_WAD)
	{
#ifdef _WIN32
		wxCSConv SJISConv(*(wxCSConv*)wxConvCurrent);
		static bool validCP932 = ::IsValidCodePage(932) != 0;
		if (validCP932)
		{
			SJISConv = wxCSConv(wxFontMapper::GetEncodingName(wxFONTENCODING_SHIFT_JIS));
		}
		else
		{
			WARN_LOG(COMMON, "Cannot Convert from Charset Windows Japanese cp 932");
		}
#else
		wxCSConv SJISConv(wxFontMapper::GetEncodingName(wxFONTENCODING_EUC_JP));
#endif

		title = wxString(item->GetName(0).c_str(), SJISConv);
	}

	else // Do the same for PAL/US Games (assuming ISO 8859-1)
	{
		title = wxString::From8BitData(item->GetName(0).c_str());
	}

	return title;
}

wxString profile_tooltip = wxTRANSLATE("Selects which game should be affected by the configuration changes done in this dialog.\nThe (Default) profile affects the standard settings used for every game.");
wxString adapter_tooltip = wxTRANSLATE("Select a hardware adapter to use.\nWhen in doubt, use the first one");
wxString ar_tooltip = wxTRANSLATE("Select what aspect ratio to use when rendering:\nAuto: Use the native aspect ratio (4:3)\nForce 16:9: Stretch the picture to an aspect ratio of 16:9.\nForce 4:3: Stretch the picture to an aspect ratio of 4:3.\nStretch to window: Stretch the picture to the window size.");
wxString ws_hack_tooltip = wxTRANSLATE("Force the game to output graphics for widescreen resolutions.\nNote that this might cause graphical glitches");
wxString vsync_tooltip = wxTRANSLATE("Wait for vertical blanks.\nReduces tearing but might also decrease performance");
wxString af_tooltip = wxTRANSLATE("Enables anisotropic filtering.\nEnhances visual quality of textures that are at oblique viewing angles.");
wxString aa_tooltip = wxTRANSLATE("Reduces the amount of aliasing caused by rasterizing 3D graphics.\nThis makes the rendered picture look less blocky but also heavily decreases performance.");
wxString native_mips_tooltip = wxTRANSLATE("Loads native mipmaps instead of generating them.\nLoading native mipmaps is the more accurate behavior, but might also decrease performance (your mileage might vary though).");
wxString scaled_efb_copy_tooltip = wxTRANSLATE("Uses the high-resolution render buffer for EFB copies instead of scaling them down to native resolution.\nVastly improves visual quality in games which use EFB copies but might cause glitches in some games.");
wxString pixel_lighting_tooltip = wxTRANSLATE("Calculates lighting of 3D graphics on a per-pixel basis rather than per vertex.\nThis is the more accurate behavior but reduces performance.");
wxString pixel_depth_tooltip = wxT("");
wxString force_filtering_tooltip = wxTRANSLATE("Forces bilinear texture filtering even if the game explicitly disabled it.\nImproves texture quality (especially when using a high internal resolution) but causes glitches in some games.");
wxString _3d_vision_tooltip = wxT("");
wxString internal_res_tooltip = wxTRANSLATE("Specifies the resolution used to render at. A high resolution will improve visual quality but is also quite heavy on performance and might cause glitches in certain games.\nFractional: Uses your display resolution directly instead of the native resolution. The quality scales with your display/window size, as does the performance impact.\nIntegral: This is like Fractional, but rounds up to an integer multiple of the native resolution. Should give a more accurate look but is usually slower.\nThe other options are fixed resolutions for choosing a visual quality independent of your display size.");
wxString efb_access_tooltip = wxTRANSLATE("Allows the CPU to read or write to the EFB (render buffer).\nThis is needed for certain gameplay functionality (e.g. star pointer in Super Mario Galaxy) as well as for certain visual effects (e.g. Monster Hunter Tri),\nbut enabling this option can also have a huge negative impact on performance if the game uses this functionality heavily.");
wxString efb_emulate_format_changes_tooltip = wxTRANSLATE("Enables reinterpreting the data inside the EFB when the pixel format changes.\nSome games depend on this function for certain effects, so enable it if you're having glitches.\nDepending on how the game uses this function, the speed hits caused by this option range from none to critical.");
wxString efb_copy_tooltip = wxTRANSLATE("Enables emulation of Embedded Frame Buffer copies, if the game uses them.\nGames often need this for post-processing or other things, but if you can live without it, you can sometimes get a big speedup.");
wxString efb_copy_texture_tooltip = wxTRANSLATE("Emulate frame buffer copies directly to textures.\nThis is not so accurate, but it's good enough for the way many games use framebuffer copies.");
wxString efb_copy_ram_tooltip = wxTRANSLATE("Fully emulate embedded frame buffer copies.\nThis is more accurate than EFB Copy to Texture, and some games need this to work properly, but it can also be very slow.");
wxString stc_tooltip = wxTRANSLATE("Keeps track of textures based on looking at the actual pixels in the texture.\nCan cause slowdown, but some games need this option enabled to work properly.");
wxString stc_speed_tooltip = wxTRANSLATE("Faster variants look at fewer pixels and thus have more potential for errors.\nSlower variants look at more pixels and thus are safer.");
wxString wireframe_tooltip = wxTRANSLATE("Render the scene as a wireframe.\nThis is only useful for debugging purposes.");
wxString disable_lighting_tooltip = wxTRANSLATE("Disable lighting. Improves performance but causes lighting to disappear in games which use it.");
wxString disable_textures_tooltip = wxTRANSLATE("Disable texturing.\nThis is only useful for debugging purposes.");
wxString disable_fog_tooltip = wxTRANSLATE("Disable fog. Improves performance but causes glitches in games which rely on proper fog emulation.");
wxString disable_alphapass_tooltip = wxTRANSLATE("Disables an alpha-setting pass.\nBreaks certain effects but might help performance.");
wxString show_fps_tooltip = wxTRANSLATE("Show the number of frames rendered per second.");
wxString show_input_display_tooltip = wxTRANSLATE("Display the inputs read by the emulator.");
wxString show_stats_tooltip = wxTRANSLATE("Show various statistics.\nThis is only useful for debugging purposes.");
wxString proj_stats_tooltip = wxTRANSLATE("Show projection statistics.\nThis is only useful for debugging purposes.");
wxString texfmt_tooltip = wxTRANSLATE("Modify textures to show the format they're using.\nThis is only useful for debugging purposes.");
wxString efb_copy_regions_tooltip = wxT("");
wxString xfb_tooltip = wxT("");
wxString dump_textures_tooltip = wxTRANSLATE("Dump game textures to User/Dump/Textures/<game id>/");
wxString load_hires_textures_tooltip = wxTRANSLATE("Load high-resolution textures from User/Load/Textures/<game id>/");
wxString dump_efb_tooltip = wxT("");
wxString dump_frames_tooltip = wxT("");
#if !defined WIN32 && defined HAVE_LIBAV
wxString use_ffv1_tooltip = wxT("");
#endif
wxString free_look_tooltip = wxT("");
wxString crop_tooltip = wxT("");
wxString opencl_tooltip = wxT("");
wxString dlc_tooltip = wxT("");
wxString omp_tooltip = wxTRANSLATE("Uses multiple threads to decode the textures in the game.");
wxString hotkeys_tooltip = wxT("");
wxString ppshader_tooltip = wxT("");
wxString cache_efb_copies_tooltip = wxTRANSLATE("When using EFB to RAM we very often need to decode RAM data to a VRAM texture, which is a very time-consuming task.\nWith this option enabled, we'll skip decoding a texture if it didn't change.\nThis results in a nice speedup, but possibly causes glitches.\nIf you have any problems with this option enabled you should either try increasing the safety of the texture cache or disable this option.\n(NOTE: The safer the texture cache is adjusted the lower the speedup will be; accurate texture cache set to \"safe\" might actually be slower!)");

wxString def_profile = wxTRANSLATE("< as Default Profile >");

// this macro decides the config which binds all controls:
// default config = main data from cur_vconfig, default and state data from g_Config
// custom config =  main data from cur_vconfig, default value from def_vconfig, state data from cur_vconfig
//
#define CONFIG(member, n) (cur_profile == 0) ? g_Config.member : (n == 0) ? def_vconfig.member : cur_vconfig.member

VideoConfigDiag::VideoConfigDiag(wxWindow* parent, const std::string &title, const std::string& _ininame)
	: wxDialog(parent, -1,
		wxString::Format(_("Dolphin %s Graphics Configuration"),
			wxGetTranslation(wxString::From8BitData(title.c_str()))),
		wxDefaultPosition, wxDefaultSize)
	, choice_adapter(NULL)
	, choice_ppshader(NULL)
	, ininame(_ininame)
	, GameListCtrl(main_frame->GetGameListCtrl())
{
	
#define SET_PARAMS(member) cur_vconfig.member, CONFIG(member, 0), CONFIG(UI_State.member, 1)

	// TODO: Make this less hacky
	cur_vconfig = g_Config; // take over backend_info (preserve integrity)

	// If a game from the game list is running, show the game specific config; show the default config otherwise
	long cb_style = wxCHK_3STATE;
	cur_profile = 0;

	if (Core::IsRunning())
	{
		// Search which ISO has been started
		for (long index = GameListCtrl->GetNextItem(-1); index != -1; index = GameListCtrl->GetNextItem(index))
		{
			const GameListItem* item = GameListCtrl->GetISO(GameListCtrl->GetItemData(index));
			if (!item) continue;
			if (item->GetUniqueID() == SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID)
			{
				cur_profile = index + 1;
				break;
			}
		}
	}
	
	prev_profile = cur_profile;

	// Load default profile settings
	def_vconfig.Load((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str());
	
	// Load current profile settings
	std::string game_ini;
	if (cur_profile != 0)
	{
		const GameListItem* item = GameListCtrl->GetISO(GameListCtrl->GetItemData(cur_profile - 1));
		game_ini = File::GetUserPath(D_GAMECONFIG_IDX) + item->GetUniqueID() + ".ini";
		cb_style |= wxCHK_ALLOW_3RD_STATE_FOR_USER;
	}

	cur_vconfig.Load((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str(), (cur_profile != 0), game_ini.c_str());

	wxNotebook* const notebook = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize);

	// -- GENERAL --
	{
	wxPanel* const page_general = new wxPanel(notebook, -1, wxDefaultPosition);
	notebook->AddPage(page_general, _("General"));
	wxBoxSizer* const szr_general = new wxBoxSizer(wxVERTICAL);

	// - basic
	{
	wxFlexGridSizer* const szr_basic = new wxFlexGridSizer(2, 5, 5);

	// game specific config
	szr_basic->Add(new wxStaticText(page_general, -1, _("Configuration profile:")), 1, wxALIGN_CENTER_VERTICAL, 5);
	profile_cb = new IntSettingChoice(page_general, cur_profile, wxGetTranslation(profile_tooltip));
	szr_basic->Add(profile_cb, 1, 0, 0);

	profile_cb->AppendString(_("(Default)"));
	for (long index = GameListCtrl->GetNextItem(-1); index != -1; index = GameListCtrl->GetNextItem(index))
	{
		const GameListItem* item = GameListCtrl->GetISO(GameListCtrl->GetItemData(index));
		if (!item) continue;
		profile_cb->AppendString(FormatString(item));
	}

	profile_cb->Select(cur_profile);
	_connect_macro_(profile_cb, VideoConfigDiag::Event_OnProfileChange, wxEVT_COMMAND_CHOICE_SELECTED, this);

	// adapter // for D3D only
	if (cur_vconfig.backend_info.Adapters.size())
	{
	szr_basic->Add(new wxStaticText(page_general, -1, _("Adapter:")), 1, wxALIGN_CENTER_VERTICAL, 5);
	choice_adapter = new IntSettingChoice(page_general, SET_PARAMS(iAdapter), cur_profile, wxGetTranslation(adapter_tooltip));

	std::vector<std::string>::const_iterator
		it = cur_vconfig.backend_info.Adapters.begin(),
		itend = cur_vconfig.backend_info.Adapters.end();
	for (; it != itend; ++it)
		choice_adapter->AppendString(wxString::FromAscii(it->c_str()));

	if (cur_profile != 0)
		choice_adapter->Insert(wxGetTranslation(def_profile), 0);

	szr_basic->Add(choice_adapter, 1, 0, 0);
	}

	// aspect-ratio
	{
	const wxString ar_choices[] = { _("Auto [recommended]"),
		_("Force 16:9"), _("Force 4:3"), _("Stretch to Window") };

	szr_basic->Add(new wxStaticText(page_general, -1, _("Aspect Ratio:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	choice_aspect = new IntSettingChoice(page_general,
		SET_PARAMS(iAspectRatio), cur_profile, wxGetTranslation(ar_tooltip), sizeof(ar_choices)/sizeof(*ar_choices), ar_choices);

	if (cur_profile != 0)
		choice_aspect->Insert(wxGetTranslation(def_profile), 0);

	szr_basic->Add(choice_aspect, 1, 0, 0);
	}

	// widescreen hack
	{
	szr_basic->AddStretchSpacer(1);
	szr_basic->Add(widescreen_hack = new SettingCheckBox(page_general, _("Widescreen Hack"), wxGetTranslation(ws_hack_tooltip), SET_PARAMS(bWidescreenHack), false, cb_style), 1, 0, 0);
	szr_basic->AddStretchSpacer(1);
	szr_basic->Add(vsync = new SettingCheckBox(page_general, _("V-Sync"), wxGetTranslation(vsync_tooltip), SET_PARAMS(bVSync), false, cb_style), 1, 0, 0);
	}

	// enhancements
	wxFlexGridSizer* const szr_enh = new wxFlexGridSizer(2, 5, 5);

	szr_enh->Add(new wxStaticText(page_general, -1, _("Anisotropic Filtering:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	const wxString af_choices[] = {wxT("1x"), wxT("2x"), wxT("4x"), wxT("8x"), wxT("16x")};
	szr_enh->Add(anisotropic_filtering = new IntSettingChoice(page_general, SET_PARAMS(iMaxAnisotropy), cur_profile, wxGetTranslation(af_tooltip), 5, af_choices));
	
	if (cur_profile != 0)
		anisotropic_filtering->Insert(wxGetTranslation(def_profile), 0);
	
	text_aamode = new wxStaticText(page_general, -1, _("Anti-Aliasing:"));
	szr_enh->Add(text_aamode, 1, wxALIGN_CENTER_VERTICAL, 0);
	choice_aamode = new IntSettingChoice(page_general, SET_PARAMS(iMultisampleMode), cur_profile, wxGetTranslation(aa_tooltip));

	std::vector<std::string>::const_iterator
		it = cur_vconfig.backend_info.AAModes.begin(),
		itend = cur_vconfig.backend_info.AAModes.end();
	for (; it != itend; ++it)
		choice_aamode->AppendString(wxGetTranslation(wxString::FromAscii(it->c_str())));

	if (cur_profile != 0)
		choice_aamode->Insert(wxGetTranslation(def_profile), 0);

	szr_enh->Add(choice_aamode);
	szr_enh->Add(native_mips = new SettingCheckBox(page_general, _("Load Native Mipmaps"), wxGetTranslation(native_mips_tooltip), SET_PARAMS(bUseNativeMips), false, cb_style));
	szr_enh->Add(efb_scaled_copy = new SettingCheckBox(page_general, _("EFB Scaled Copy"), wxGetTranslation(scaled_efb_copy_tooltip), SET_PARAMS(bCopyEFBScaled), false, cb_style));
	szr_enh->Add(pixel_lighting = new SettingCheckBox(page_general, _("Pixel Lighting"), wxGetTranslation(pixel_lighting_tooltip), SET_PARAMS(bEnablePixelLighting), false, cb_style));
	szr_enh->Add(pixel_depth =  new SettingCheckBox(page_general, _("Pixel Depth"), wxGetTranslation(pixel_depth_tooltip), SET_PARAMS(bEnablePerPixelDepth), false, cb_style));
	szr_enh->Add(force_filtering = new SettingCheckBox(page_general, _("Force Bi/Trilinear Filtering"), wxGetTranslation(force_filtering_tooltip), SET_PARAMS(bForceFiltering), false, cb_style));
	
	_3d_vision = new SettingCheckBox(page_general, _("3D Vision (Requires Fullscreen)"), wxGetTranslation(_3d_vision_tooltip), SET_PARAMS(b3DVision), false, cb_style);
	szr_enh->Add(_3d_vision);

	// - EFB
	// EFB scale
	wxBoxSizer* const efb_scale_szr = new wxBoxSizer(wxHORIZONTAL);
	const wxString efbscale_choices[] = { _("Fractional"), _("Integral [recommended]"),
		wxT("1x"), wxT("2x"), wxT("3x"), wxT("0.75x"), wxT("0.5x"), wxT("0.375x") };

	choice_efbscale = new IntSettingChoice(page_general,
			SET_PARAMS(iEFBScale), cur_profile, wxGetTranslation(internal_res_tooltip), sizeof(efbscale_choices)/sizeof(*efbscale_choices), efbscale_choices);

	if (cur_profile != 0)
		choice_efbscale->Insert(wxGetTranslation(def_profile), 0);

	efb_scale_szr->Add(new wxStaticText(page_general, -1, _("Scale:")), 0, wxALIGN_CENTER_VERTICAL, 5);
	efb_scale_szr->Add(choice_efbscale, 1,  wxLEFT, 5);

	emulate_efb_format_changes = new SettingCheckBox(page_general, _("Emulate format changes"), wxGetTranslation(efb_emulate_format_changes_tooltip), SET_PARAMS(bEFBEmulateFormatChanges), false, cb_style);

	// EFB copy
	efbcopy_enable = new SettingCheckBox(page_general, _("Enable"), wxGetTranslation(efb_copy_tooltip), SET_PARAMS(bEFBCopyEnable), false, cb_style);
	efbcopy_texture = new SettingRadioButton(page_general, _("Texture"), wxGetTranslation(efb_copy_texture_tooltip), cur_vconfig.bCopyEFBToTexture, false, wxRB_GROUP);
	efbcopy_ram = new SettingRadioButton(page_general, _("RAM"), wxGetTranslation(efb_copy_ram_tooltip), cur_vconfig.bCopyEFBToTexture, true);
	cache_efb_copies = new SettingCheckBox(page_general, _("Enable cache"), wxGetTranslation(cache_efb_copies_tooltip), SET_PARAMS(bEFBCopyCacheEnable), false, cb_style);
	wxStaticBoxSizer* const group_efbcopy = new wxStaticBoxSizer(wxHORIZONTAL, page_general, _("Copy"));
	group_efbcopy->Add(efbcopy_enable, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_efbcopy->AddStretchSpacer(1);
	group_efbcopy->Add(efbcopy_texture, 0, wxRIGHT, 5);
	group_efbcopy->Add(efbcopy_ram, 0, wxRIGHT, 5);
	group_efbcopy->Add(cache_efb_copies, 0, wxRIGHT, 5);


	// - safe texture cache
	stc_enable = new SettingCheckBox(page_general, _("Enable"), wxGetTranslation(stc_tooltip), SET_PARAMS(bSafeTextureCache), false, cb_style);

	stc_safe = new wxRadioButton(page_general, -1, _("Safe"),
		wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	stc_safe->SetToolTip(wxGetTranslation(stc_speed_tooltip));
	_connect_macro_(stc_safe, VideoConfigDiag::Event_StcSafe, wxEVT_COMMAND_RADIOBUTTON_SELECTED, this);

	stc_normal = new wxRadioButton(page_general, -1, _("Normal"));
	stc_normal->SetToolTip(wxGetTranslation(stc_speed_tooltip));
	_connect_macro_(stc_normal, VideoConfigDiag::Event_StcNormal, wxEVT_COMMAND_RADIOBUTTON_SELECTED, this);

	stc_fast = new wxRadioButton(page_general, -1, _("Fast"));
	stc_fast->SetToolTip(wxGetTranslation(stc_speed_tooltip));
	_connect_macro_(stc_fast, VideoConfigDiag::Event_StcFast, wxEVT_COMMAND_RADIOBUTTON_SELECTED, this);


	wxStaticBoxSizer* const group_basic = new wxStaticBoxSizer(wxVERTICAL, page_general, _("Basic"));
	szr_general->Add(group_basic, 0, wxEXPAND | wxALL, 5);
	group_basic->Add(szr_basic, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxStaticBoxSizer* const group_enh = new wxStaticBoxSizer(wxVERTICAL, page_general, _("Enhancements"));
	szr_general->Add(group_enh, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_enh->Add(szr_enh, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	wxStaticBoxSizer* const group_efb = new wxStaticBoxSizer(wxVERTICAL, page_general, _("EFB"));
	szr_general->Add(group_efb, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_efb->Add(efb_scale_szr, 0, wxBOTTOM | wxLEFT, 5);
	group_efb->Add(efbaccess_enable = new SettingCheckBox(page_general, _("Enable CPU Access"), wxGetTranslation(efb_access_tooltip), SET_PARAMS(bEFBAccessEnable), false, cb_style), 0, wxBOTTOM | wxLEFT, 5);
	group_efb->Add(emulate_efb_format_changes, 0, wxBOTTOM | wxLEFT, 5);
	group_efb->Add(group_efbcopy, 0, wxEXPAND | wxBOTTOM, 5);

	wxStaticBoxSizer* const group_safetex = new wxStaticBoxSizer(wxHORIZONTAL, page_general, _("Accurate Texture Cache"));
	szr_general->Add(group_safetex, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_safetex->Add(stc_enable, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_safetex->AddStretchSpacer(1);
	group_safetex->Add(stc_safe, 0, wxRIGHT, 5);
	group_safetex->Add(stc_normal, 0, wxRIGHT, 5);
	group_safetex->Add(stc_fast, 0, wxRIGHT, 5);
	}

	page_general->SetSizerAndFit(szr_general);
	}

	// -- ADVANCED --
	{
	wxPanel* const page_advanced = new wxPanel(notebook, -1, wxDefaultPosition);
	notebook->AddPage(page_advanced, _("Advanced"));
	wxBoxSizer* const szr_advanced = new wxBoxSizer(wxVERTICAL);

	// configuration profiles
	{
	wxStaticBoxSizer* const group_profile = new wxStaticBoxSizer(wxHORIZONTAL, page_advanced, _("Configuration profile"));
	profile_text = new wxStaticText(page_advanced, -1, profile_cb->GetStringSelection());
	szr_advanced->Add(group_profile, 0, wxEXPAND | wxALL, 5);
	group_profile->Add(profile_text, 1, wxEXPAND | wxALL, 5);
	}

	// - rendering
	{
	wxGridSizer* const szr_rendering = new wxGridSizer(2, 5, 5);

	szr_rendering->Add(wireframe = new SettingCheckBox(page_advanced, _("Enable Wireframe"), wxGetTranslation(wireframe_tooltip), SET_PARAMS(bWireFrame), false, cb_style));
	szr_rendering->Add(disable_lighting = new SettingCheckBox(page_advanced, _("Disable Lighting"), wxGetTranslation(disable_lighting_tooltip), SET_PARAMS(bDisableLighting), false, cb_style));
	szr_rendering->Add(disable_textures = new SettingCheckBox(page_advanced, _("Disable Textures"), wxGetTranslation(disable_textures_tooltip), SET_PARAMS(bDisableTexturing), false, cb_style));
	szr_rendering->Add(disable_fog = new SettingCheckBox(page_advanced, _("Disable Fog"), wxGetTranslation(disable_fog_tooltip), SET_PARAMS(bDisableFog), false, cb_style));
	szr_rendering->Add(disable_dst_alpha = new SettingCheckBox(page_advanced, _("Disable Dest. Alpha Pass"), wxGetTranslation(disable_alphapass_tooltip), SET_PARAMS(bDstAlphaPass), false, cb_style));

	wxStaticBoxSizer* const group_rendering = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Rendering"));
	szr_advanced->Add(group_rendering, 0, wxEXPAND | wxALL, 5);
	group_rendering->Add(szr_rendering, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	}

	// - info
	{
	wxGridSizer* const szr_info = new wxGridSizer(2, 5, 5);

	szr_info->Add(show_fps = new SettingCheckBox(page_advanced, _("Show FPS"), wxGetTranslation(show_fps_tooltip), SET_PARAMS(bShowFPS), false, cb_style));
	szr_info->Add(overlay_stats = new SettingCheckBox(page_advanced, _("Various Statistics"), wxGetTranslation(show_stats_tooltip), SET_PARAMS(bOverlayStats), false, cb_style));
	szr_info->Add(overlay_proj_stats = new SettingCheckBox(page_advanced, _("Projection Stats"), wxGetTranslation(proj_stats_tooltip), SET_PARAMS(bOverlayProjStats), false, cb_style));
	szr_info->Add(texfmt_overlay = new SettingCheckBox(page_advanced, _("Texture Format"), wxGetTranslation(texfmt_tooltip), SET_PARAMS(bTexFmtOverlayEnable), false, cb_style));
	szr_info->Add(efb_copy_regions = new SettingCheckBox(page_advanced, _("EFB Copy Regions"), wxGetTranslation(efb_copy_regions_tooltip), SET_PARAMS(bShowEFBCopyRegions), false, cb_style));
	szr_info->Add(show_shader_errors = new SettingCheckBox(page_advanced, _("Show Shader Errors"), wxT(""), SET_PARAMS(bShowShaderErrors), false, cb_style));
	szr_info->Add(show_input_display = new SettingCheckBox(page_advanced, _("Show Input Display"), wxGetTranslation(show_input_display_tooltip), SET_PARAMS(bShowInputDisplay), false, cb_style));

	wxStaticBoxSizer* const group_info = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Information"));
	szr_advanced->Add(group_info, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_info->Add(szr_info, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	}
	
	// - XFB
	{
	enable_xfb = new SettingCheckBox(page_advanced, _("Enable"), wxGetTranslation(xfb_tooltip), SET_PARAMS(bUseXFB), false, cb_style);
	virtual_xfb = new SettingRadioButton(page_advanced, _("Virtual"), wxGetTranslation(xfb_tooltip), cur_vconfig.bUseRealXFB, true, wxRB_GROUP);
	real_xfb = new SettingRadioButton(page_advanced, _("Real"), wxGetTranslation(xfb_tooltip), cur_vconfig.bUseRealXFB);

	wxStaticBoxSizer* const group_xfb = new wxStaticBoxSizer(wxHORIZONTAL, page_advanced, _("XFB"));
	szr_advanced->Add(group_xfb, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_xfb->Add(enable_xfb, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_xfb->AddStretchSpacer(1);
	group_xfb->Add(virtual_xfb, 0, wxRIGHT, 5);
	group_xfb->Add(real_xfb, 0, wxRIGHT, 5);


	}	// xfb

	// - utility
	{
	wxGridSizer* const szr_utility = new wxGridSizer(2, 5, 5);

	szr_utility->Add(dump_textures = new SettingCheckBox(page_advanced, _("Dump Textures"), wxGetTranslation(dump_textures_tooltip), SET_PARAMS(bDumpTextures), false, cb_style));
	szr_utility->Add(hires_textures = new SettingCheckBox(page_advanced, _("Load Hi-Res Textures"), wxGetTranslation(load_hires_textures_tooltip), SET_PARAMS(bHiresTextures), false, cb_style));
	szr_utility->Add(dump_efb = new SettingCheckBox(page_advanced, _("Dump EFB Target"), dump_efb_tooltip, SET_PARAMS(bDumpEFBTarget), false, cb_style));
	szr_utility->Add(dump_frames = new SettingCheckBox(page_advanced, _("Dump Frames"), dump_frames_tooltip, SET_PARAMS(bDumpFrames), false, cb_style));
	szr_utility->Add(free_look = new SettingCheckBox(page_advanced, _("Free Look"), free_look_tooltip, SET_PARAMS(bFreeLook), false, cb_style));
#if !defined WIN32 && defined HAVE_LIBAV
	szr_utility->Add(frame_dumps_via_ffv1 = new SettingCheckBox(page_advanced, _("Frame dumps use FFV1"), use_ffv1_tooltip, SET_PARAMS(bUseFFV1), false, cb_style));
#endif

	wxStaticBoxSizer* const group_utility = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Utility"));
	szr_advanced->Add(group_utility, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_utility->Add(szr_utility, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	}

	// - misc
	{
	wxGridSizer* const szr_misc = new wxGridSizer(2, 5, 5);

	szr_misc->Add(crop = new SettingCheckBox(page_advanced, _("Crop"), crop_tooltip, SET_PARAMS(bCrop), false, cb_style));
	szr_misc->Add(opencl = new SettingCheckBox(page_advanced, _("Enable OpenCL"), opencl_tooltip, SET_PARAMS(bEnableOpenCL), false, cb_style));
	szr_misc->Add(dlcache = new SettingCheckBox(page_advanced, _("Enable Display List Caching"), dlc_tooltip, SET_PARAMS(bDlistCachingEnable), false, cb_style));
	szr_misc->Add(hotkeys = new SettingCheckBox(page_advanced, _("Enable Hotkeys"), hotkeys_tooltip, SET_PARAMS(bOSDHotKey), false, cb_style));
	szr_misc->Add(ompdecoder = new SettingCheckBox(page_advanced, _("OpenMP Texture Decoder"), wxGetTranslation(omp_tooltip), SET_PARAMS(bOMPDecoder), false, cb_style));

	// postproc shader
	wxBoxSizer* const box_pps = new wxBoxSizer(wxHORIZONTAL);
	if (cur_vconfig.backend_info.PPShaders.size())
	{
		wxFlexGridSizer* const szr_pps = new wxFlexGridSizer(2, 5, 5);
		szr_pps->AddStretchSpacer(); szr_pps->AddStretchSpacer();
		szr_pps->Add(new wxStaticText(page_advanced, -1, _("Post-Processing Shader:")), 1, wxALIGN_CENTER_VERTICAL, 0);

		choice_ppshader = new StringSettingChoice(page_advanced, SET_PARAMS(sPostProcessingShader), cur_profile, wxGetTranslation(ppshader_tooltip));

		choice_ppshader->AppendString(_("(off)"));

		std::vector<std::string>::const_iterator
			it = cur_vconfig.backend_info.PPShaders.begin(),
			itend = cur_vconfig.backend_info.PPShaders.end();
		for (; it != itend; ++it)
			choice_ppshader->AppendString(wxString::FromAscii(it->c_str()));

		if (cur_profile != 0)
			choice_ppshader->Insert(wxGetTranslation(def_profile), 0);

		szr_pps->Add(choice_ppshader);
		box_pps->Add(szr_pps, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	}

	wxStaticBoxSizer* const group_misc = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Misc"));
	szr_advanced->Add(group_misc, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_misc->Add(szr_misc, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_misc->Add(box_pps);
	}

	page_advanced->SetSizerAndFit(szr_advanced);
	}

	wxSizer* sButtons = CreateButtonSizer(wxNO_DEFAULT);
	btn_default = new wxButton(this, wxID_ANY, _("Set All to Default"), wxDefaultPosition);
	btn_default->Enable(cur_profile != 0);
	_connect_macro_(btn_default, VideoConfigDiag::Event_ClickDefault, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wxButton* const btn_close = new wxButton(this, wxID_OK, _("Close"), wxDefaultPosition);
	_connect_macro_(btn_close, VideoConfigDiag::Event_ClickClose, wxEVT_COMMAND_BUTTON_CLICKED, this);

	Connect(wxID_ANY, wxEVT_CLOSE_WINDOW, wxCloseEventHandler(VideoConfigDiag::Event_Close), (wxObject*)0, this);

	sButtons->Prepend(btn_default, 0, wxALL, 5);
	sButtons->Add(btn_close, 0, wxALL, 5);

	wxBoxSizer* const szr_main = new wxBoxSizer(wxVERTICAL);
	szr_main->Add(notebook, 1, wxEXPAND | wxALL, 5);
	szr_main->Add(sButtons, 0, wxEXPAND, 5);

	SetSizerAndFit(szr_main);
	Center();
	CenterCoords = GetScreenPosition();
	SetFocus();

	SetUIValuesFromConfig();
	Connect(wxID_ANY, wxEVT_UPDATE_UI, wxUpdateUIEventHandler(VideoConfigDiag::OnUpdateUI), NULL, this);
	UpdateWindowUI();
}

void VideoConfigDiag::ChangeStyle()
{
	// here where we change opportune UI Controls only if we come from, or we go to, default profile
	if (prev_profile != cur_profile && !(prev_profile && cur_profile))
	{

		const long cb_style = (cur_profile == 0) ? wxCHK_3STATE : wxCHK_3STATE|wxCHK_ALLOW_3RD_STATE_FOR_USER;

		// set State and Rebind all controls
		#define CHANGE_DATAREF(ctrl, member) { void *p;\
			p = &(CONFIG(member, 0)); ctrl->ChangeRefDataMember(Def_Data,p);\
			p = &(CONFIG(UI_State.member, 1)); ctrl->ChangeRefDataMember(State,p);\
			if (sizeof(cur_vconfig.member) == sizeof(bool)) ctrl->SetWindowStyle(cb_style); }
		
		if (cur_vconfig.backend_info.Adapters.size())
		{
			if (cur_profile == 0) choice_adapter->Delete(0);
			else choice_adapter->Insert(wxGetTranslation(def_profile), 0);

			choice_adapter->GetParent()->Layout(); // redraws all elements inside the parent container
			CHANGE_DATAREF(choice_adapter, iAdapter);
		}
		if (cur_profile == 0) choice_aspect->Delete(0);
		else choice_aspect->Insert(wxGetTranslation(def_profile), 0);
		choice_aspect->GetParent()->Layout();
		CHANGE_DATAREF(choice_aspect, iAspectRatio);

		CHANGE_DATAREF(widescreen_hack, bWidescreenHack);
		CHANGE_DATAREF(vsync, bVSync);

		if (cur_profile == 0) anisotropic_filtering->Delete(0);
		else anisotropic_filtering->Insert(wxGetTranslation(def_profile), 0);
		anisotropic_filtering->GetParent()->Layout();
		CHANGE_DATAREF(anisotropic_filtering, iMaxAnisotropy);

		if (cur_profile == 0) choice_aamode->Delete(0);
		else choice_aamode->Insert(wxGetTranslation(def_profile), 0);
		choice_aamode->GetParent()->Layout();
		CHANGE_DATAREF(choice_aamode, iMultisampleMode);

		CHANGE_DATAREF(native_mips, bUseNativeMips);
		CHANGE_DATAREF(efb_scaled_copy, bCopyEFBScaled);
		CHANGE_DATAREF(pixel_lighting, bEnablePixelLighting);
		CHANGE_DATAREF(pixel_depth, bEnablePerPixelDepth);
		CHANGE_DATAREF(force_filtering, bForceFiltering);
		CHANGE_DATAREF(_3d_vision, b3DVision);

		if (cur_profile == 0) choice_efbscale->Delete(0);
		else choice_efbscale->Insert(wxGetTranslation(def_profile), 0);
		choice_efbscale->GetParent()->Layout();
		CHANGE_DATAREF(choice_efbscale, iEFBScale);

		CHANGE_DATAREF(efbaccess_enable, bEFBAccessEnable);
		CHANGE_DATAREF(emulate_efb_format_changes, bEFBEmulateFormatChanges);
		CHANGE_DATAREF(efbcopy_enable, bEFBCopyEnable);
		CHANGE_DATAREF(cache_efb_copies, bEFBCopyCacheEnable);
		CHANGE_DATAREF(stc_enable, bSafeTextureCache);
		CHANGE_DATAREF(wireframe, bWireFrame);
		CHANGE_DATAREF(disable_lighting, bDisableLighting);
		CHANGE_DATAREF(disable_textures, bDisableTexturing);
		CHANGE_DATAREF(disable_fog, bDisableFog);
		CHANGE_DATAREF(disable_dst_alpha, bDstAlphaPass);
		CHANGE_DATAREF(show_fps, bShowFPS);
		CHANGE_DATAREF(overlay_stats, bOverlayStats);
		CHANGE_DATAREF(overlay_proj_stats, bOverlayProjStats);
		CHANGE_DATAREF(texfmt_overlay, bTexFmtOverlayEnable);
		CHANGE_DATAREF(efb_copy_regions, bShowEFBCopyRegions);
		CHANGE_DATAREF(show_shader_errors, bShowShaderErrors);
		CHANGE_DATAREF(show_input_display, bShowInputDisplay);
		CHANGE_DATAREF(enable_xfb, bUseXFB);
		CHANGE_DATAREF(dump_textures, bDumpTextures);
		CHANGE_DATAREF(hires_textures, bHiresTextures);
		CHANGE_DATAREF(dump_efb, bDumpEFBTarget);
		CHANGE_DATAREF(dump_frames, bDumpFrames);
		CHANGE_DATAREF(free_look, bFreeLook);

	#if !defined WIN32 && defined HAVE_LIBAV
		CHANGE_DATAREF(frame_dumps_via_ffv1,bUseFFV1);
	#endif

		CHANGE_DATAREF(hotkeys, bOSDHotKey);
		CHANGE_DATAREF(dlcache, bDlistCachingEnable);
		CHANGE_DATAREF(ompdecoder, bOMPDecoder);
		CHANGE_DATAREF(opencl, bEnableOpenCL);
		CHANGE_DATAREF(crop, bCrop);
		
		if (cur_vconfig.backend_info.PPShaders.size())
		{
			if (cur_profile == 0)
				choice_ppshader->Delete(0);
			else
				choice_ppshader->Insert(wxGetTranslation(def_profile), 0);

			choice_ppshader->GetParent()->Layout();
			CHANGE_DATAREF(choice_ppshader, sPostProcessingShader);
		}
		
		Fit(); // wraps sizes of the outer layout
		if (CenterCoords == this->GetScreenPosition())
		{
			Center(); // lastly if window hasn't moved, re-center it
			CenterCoords = this->GetScreenPosition();
		}
	}
}

void VideoConfigDiag::Event_OnProfileChange(wxCommandEvent& ev)
{
	// Save settings of current profile
	if (cur_profile == 0)
	{
		def_vconfig = cur_vconfig; // copy default profile changes
		cur_vconfig.Save((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str());
	}
	else
	{
		const GameListItem* item = GameListCtrl->GetISO(GameListCtrl->GetItemData(cur_profile - 1));
		cur_vconfig.GameIniSave((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str(), 
				(std::string(File::GetUserPath(D_GAMECONFIG_IDX)) + item->GetUniqueID() + ".ini").c_str());
	}

	// Enable new profile
	cur_profile = ev.GetInt();

	btn_default->Enable(cur_profile != 0);
	// Reset settings and, if necessary, load game-specific settings
	std::string game_ini;
	if (cur_profile != 0)
	{
		const GameListItem* item = GameListCtrl->GetISO(GameListCtrl->GetItemData(cur_profile - 1));
		game_ini = File::GetUserPath(D_GAMECONFIG_IDX) + item->GetUniqueID() + ".ini";
	}

	cur_vconfig.Load((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str(), (cur_profile != 0), game_ini.c_str());

	ChangeStyle();
	prev_profile = cur_profile;

	// Update our UI elements with the new config
	SetUIValuesFromConfig();
	UpdateWindowUI();
	profile_text->SetLabel(profile_cb->GetStringSelection());

	ev.Skip();
}

void VideoConfigDiag::OnUpdateUI(wxUpdateUIEvent& ev)
{
	bool enable_group;

	// Anti-aliasing
	choice_aamode->Enable(cur_vconfig.backend_info.AAModes.size() > 1);
	text_aamode->Enable(cur_vconfig.backend_info.AAModes.size() > 1);

	// pixel lighting
	pixel_lighting->Enable(cur_vconfig.backend_info.bSupportsPixelLighting);

	// 3D vision
	_3d_vision->Show(cur_vconfig.backend_info.bSupports3DVision);

	// EFB copy
	enable_group = cur_vconfig.bEFBCopyEnable && (efbcopy_enable->Get3StateValue() != wxCHK_UNDETERMINED);
	efbcopy_texture->Enable(enable_group);
	efbcopy_ram->Enable(enable_group);
	cache_efb_copies->Enable(cur_vconfig.bEFBCopyEnable && !cur_vconfig.bCopyEFBToTexture);

	// EFB format change emulation
	emulate_efb_format_changes->Enable(cur_vconfig.backend_info.bSupportsFormatReinterpretation);

	// ATC
	enable_group = cur_vconfig.bSafeTextureCache && (stc_enable->Get3StateValue() != wxCHK_UNDETERMINED);
	stc_safe->Enable(enable_group);
	stc_normal->Enable(enable_group);
	stc_fast->Enable(enable_group);

	// XFB
	enable_group = cur_vconfig.bUseXFB && (enable_xfb->Get3StateValue() != wxCHK_UNDETERMINED);
	virtual_xfb->Enable(enable_group);
	real_xfb->Enable(enable_group);

	// here where we check Radio Button Groups and if main CheckBox has 3rd state activated
	// NOTE: this code block just before g_Config is updated
	if (cur_profile != 0)
	{
		// ID check is to reduce drastically the CPU load, since too many SetValue() calls at once,
		// are very expensive inside this procedure
		if (ev.GetId() == efbcopy_enable->GetId() && efbcopy_enable->Get3StateValue() == wxCHK_UNDETERMINED)
		{
			cur_vconfig.bCopyEFBToTexture = def_vconfig.bCopyEFBToTexture;
			efbcopy_texture->SetValue(cur_vconfig.bCopyEFBToTexture);
			efbcopy_ram->SetValue(!cur_vconfig.bCopyEFBToTexture);
		}
		if (ev.GetId() == stc_enable->GetId() && stc_enable->Get3StateValue() == wxCHK_UNDETERMINED)
		{
			cur_vconfig.iSafeTextureCache_ColorSamples = def_vconfig.iSafeTextureCache_ColorSamples;
			stc_safe->SetValue(0 == cur_vconfig.iSafeTextureCache_ColorSamples);
			stc_normal->SetValue(512 == cur_vconfig.iSafeTextureCache_ColorSamples);
			stc_fast->SetValue(128 == cur_vconfig.iSafeTextureCache_ColorSamples);
		}
		if (ev.GetId() == enable_xfb->GetId() && enable_xfb->Get3StateValue() == wxCHK_UNDETERMINED)
		{
			cur_vconfig.bUseRealXFB = def_vconfig.bUseRealXFB;
			virtual_xfb->SetValue(!cur_vconfig.bUseRealXFB);
			real_xfb->SetValue(cur_vconfig.bUseRealXFB);
		}
	}
	
	// If emulation hasn't started, yet, always update g_Config.
	// Otherwise only update it if we're editing the currently running game's profile
	if (!Core::IsRunning())
	{
		g_Config = cur_vconfig;
	}
	else if (cur_profile != 0)
	{
		if (GameListCtrl->GetISO(GameListCtrl->GetItemData(cur_profile - 1))->GetUniqueID() ==
					SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID)
			g_Config = cur_vconfig;
	}
	else // here, if we're editing the Default profile and a game is running...
	{
		// RadioButton instant update from Default profile
		if (!g_Config.UI_State.bSafeTextureCache)
			g_Config.iSafeTextureCache_ColorSamples = cur_vconfig.iSafeTextureCache_ColorSamples;

		if (!g_Config.UI_State.bUseXFB)
			g_Config.bUseRealXFB = cur_vconfig.bUseRealXFB;

		if (!g_Config.UI_State.bEFBCopyEnable)
			g_Config.bCopyEFBToTexture = cur_vconfig.bCopyEFBToTexture;

		// other controls (checkbox, choice dropdown) handle 'the instant update' internally inside their own class
	}
	ev.Skip();
}

void VideoConfigDiag::SetUIValuesFromConfig()
{
	
	const int inc = (cur_profile != 0) ? 1 : 0;
	
	#define SET_CHOICE(control, member) {\
		void *p = control; void *m = &cur_vconfig.member;\
		switch (sizeof(cur_vconfig.member)) {\
		case sizeof(bool): if (cur_vconfig.UI_State.member) ((SettingCheckBox*)p)->Set3StateValue((wxCheckBoxState)*(bool*)m);\
			else ((SettingCheckBox*)p)->Set3StateValue(wxCHK_UNDETERMINED); break;\
		case sizeof(int): if (cur_vconfig.UI_State.member) ((IntSettingChoice*)p)->SetSelection(*(int*)m + inc);\
			else ((IntSettingChoice*)p)->SetSelection(0); break; } }

	if (choice_adapter) SET_CHOICE(choice_adapter, iAdapter);
	SET_CHOICE(choice_aspect, iAspectRatio);
	SET_CHOICE(widescreen_hack, bWidescreenHack);
	SET_CHOICE(vsync, bVSync);

	SET_CHOICE(anisotropic_filtering, iMaxAnisotropy);
	SET_CHOICE(choice_aamode, iMultisampleMode);

	SET_CHOICE(native_mips, bUseNativeMips);
	SET_CHOICE(efb_scaled_copy,bCopyEFBScaled);
	SET_CHOICE(pixel_lighting, bEnablePixelLighting);
	SET_CHOICE(pixel_depth, bEnablePerPixelDepth);
	SET_CHOICE(force_filtering, bForceFiltering);
	SET_CHOICE(_3d_vision, b3DVision);

	SET_CHOICE(choice_efbscale, iEFBScale);
	SET_CHOICE(efbaccess_enable, bEFBAccessEnable);
	SET_CHOICE(emulate_efb_format_changes, bEFBEmulateFormatChanges);

	SET_CHOICE(efbcopy_enable, bEFBCopyEnable);
	efbcopy_texture->SetValue(cur_vconfig.bCopyEFBToTexture);
	efbcopy_ram->SetValue(!cur_vconfig.bCopyEFBToTexture);
	SET_CHOICE(cache_efb_copies, bEFBCopyCacheEnable);

	SET_CHOICE(stc_enable, bSafeTextureCache);
	stc_safe->SetValue(0 == cur_vconfig.iSafeTextureCache_ColorSamples);
	stc_normal->SetValue(512 == cur_vconfig.iSafeTextureCache_ColorSamples);
	stc_fast->SetValue(128 == cur_vconfig.iSafeTextureCache_ColorSamples);

	SET_CHOICE(wireframe, bWireFrame);
	SET_CHOICE(disable_lighting, bDisableLighting);
	SET_CHOICE(disable_textures, bDisableTexturing);
	SET_CHOICE(disable_fog, bDisableFog);
	SET_CHOICE(disable_dst_alpha, bDstAlphaPass);

	SET_CHOICE(show_fps, bShowFPS);
	SET_CHOICE(overlay_stats, bOverlayStats);
	SET_CHOICE(overlay_proj_stats, bOverlayProjStats);
	SET_CHOICE(texfmt_overlay, bTexFmtOverlayEnable);
	SET_CHOICE(efb_copy_regions, bShowEFBCopyRegions);
	SET_CHOICE(show_shader_errors, bShowShaderErrors);
	SET_CHOICE(show_input_display, bShowInputDisplay);

	SET_CHOICE(enable_xfb, bUseXFB);
	virtual_xfb->SetValue(!cur_vconfig.bUseRealXFB);
	real_xfb->SetValue(cur_vconfig.bUseRealXFB);

	SET_CHOICE(dump_textures, bDumpTextures);
	SET_CHOICE(hires_textures, bHiresTextures);
	SET_CHOICE(dump_efb, bDumpEFBTarget);
	SET_CHOICE(dump_frames, bDumpFrames);
	SET_CHOICE(free_look, bFreeLook);
#if !defined WIN32 && defined HAVE_LIBAV
	SET_CHOICE(frame_dumps_via_ffv1, bUseFFV1);
#endif

	SET_CHOICE(crop, bCrop);
	SET_CHOICE(opencl, bEnableOpenCL);
	SET_CHOICE(dlcache, bDlistCachingEnable);
	SET_CHOICE(ompdecoder, bOMPDecoder);
	SET_CHOICE(hotkeys, bOSDHotKey);
	if (choice_ppshader)
	{
		std::string m = cur_vconfig.sPostProcessingShader;
		if (cur_vconfig.UI_State.sPostProcessingShader)
		{
			if (m.empty()) choice_ppshader->SetSelection(inc);
			else choice_ppshader->SetStringSelection(wxString::FromAscii(m.c_str()));
		}
			else choice_ppshader->SetSelection(0);
	}
}
