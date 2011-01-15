
#include "VideoConfigDiag.h"

#include "FileUtil.h"

#include <wx/intl.h>

#define _connect_macro_(b, f, c, s)	(b)->Connect(wxID_ANY, (c), wxCommandEventHandler( f ), (wxObject*)0, (wxEvtHandler*)s)

// template instantiation
template class BoolSetting<wxCheckBox>;
template class BoolSetting<wxRadioButton>;

template <>
SettingCheckBox::BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse, long style)
	: wxCheckBox(parent, -1, label, wxDefaultPosition, wxDefaultSize, style)
	, m_setting(setting)
	, m_reverse(reverse)
{
	SetToolTip(tooltip);
	SetValue(m_setting ^ m_reverse);
	_connect_macro_(this, SettingCheckBox::UpdateValue, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
}

template <>
SettingRadioButton::BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse, long style)
	: wxRadioButton(parent, -1, label, wxDefaultPosition, wxDefaultSize, style)
	, m_setting(setting)
	, m_reverse(reverse)
{
	SetToolTip(tooltip);
	SetValue(m_setting ^ m_reverse);
	_connect_macro_(this, SettingRadioButton::UpdateValue, wxEVT_COMMAND_RADIOBUTTON_SELECTED, this);
}

SettingChoice::SettingChoice(wxWindow* parent, int &setting, const wxString& tooltip, int num, const wxString choices[])
	: wxChoice(parent, -1, wxDefaultPosition, wxDefaultSize, num, choices)
	, m_setting(setting)
{
	SetToolTip(tooltip);
	Select(m_setting);
	_connect_macro_(this, SettingChoice::UpdateValue, wxEVT_COMMAND_CHOICE_SELECTED, this);
}

void SettingChoice::UpdateValue(wxCommandEvent& ev)
{
	m_setting = ev.GetInt();
	ev.Skip();
}

void VideoConfigDiag::Event_ClickClose(wxCommandEvent&)
{
	Close();
}

void VideoConfigDiag::Event_Close(wxCloseEvent& ev)
{
	g_Config.Save((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str());

	ev.Skip();
}


// TODO: implement some hack to increase the tooltip display duration, because some of these are way too long for anyone to read in 5 seconds.

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
wxString show_stats_tooltip = wxTRANSLATE("Show various statistics.\nThis is only useful for debugging purposes.");
wxString proj_stats_tooltip = wxTRANSLATE("Show projection statistics.\nThis is only useful for debugging purposes.");
wxString texfmt_tooltip = wxTRANSLATE("Modify textures to show the format they're using.\nThis is only useful for debugging purposes.");
wxString efb_copy_regions_tooltip = wxT("");
wxString xfb_tooltip = wxT("");
wxString dump_textures_tooltip = wxTRANSLATE("Dump game textures to User/Dump/Textures/<game id>/");
wxString load_hires_textures_tooltip = wxTRANSLATE("Load high-resolution textures from User/Load/Textures/<game id>/");
wxString dump_efb_tooltip = wxT("");
wxString dump_frames_tooltip = wxT("");
wxString free_look_tooltip = wxT("");
wxString crop_tooltip = wxT("");
wxString opencl_tooltip = wxT("");
wxString dlc_tooltip = wxT("");
wxString hotkeys_tooltip = wxT("");
wxString adjust_window_size_tooltip = wxTRANSLATE("Adjusts the window to match the game's output resolution scaled by the EFB scale.\nIt is best to set the aspect ratio to stretch when using this.");
wxString ppshader_tooltip = wxT("");
wxString cache_efb_copies_tooltip = wxTRANSLATE("When using EFB to RAM we very often need to decode RAM data to a VRAM texture, which is a very time-consuming task.\nWith this option enabled, we'll skip decoding a texture if it didn't change.\nThis results in a nice speedup, but possibly causes glitches.\nIf you have any problems with this option enabled you should either try increasing the safety of the texture cache or disable this option.\n(NOTE: The safier the texture cache is adjusted the lower the speedup will be; accurate texture cache set to \"safe\" might actually be slower!)");

VideoConfigDiag::VideoConfigDiag(wxWindow* parent, const std::string &title, const std::string& _ininame)
	: wxDialog(parent, -1,
		wxString::Format(_("Dolphin %s Graphics Configuration"),
			wxGetTranslation(wxString::From8BitData(title.c_str()))),
		wxDefaultPosition, wxDefaultSize)
	, vconfig(g_Config)
	, ininame(_ininame)
{
	vconfig.Load((File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini").c_str());

	wxNotebook* const notebook = new wxNotebook(this, -1, wxDefaultPosition, wxDefaultSize);

	// -- GENERAL --
	{
	wxPanel* const page_general = new wxPanel(notebook, -1, wxDefaultPosition);
	notebook->AddPage(page_general, _("General"));
	wxBoxSizer* const szr_general = new wxBoxSizer(wxVERTICAL);

	// - basic
	{
	wxStaticBoxSizer* const group_basic = new wxStaticBoxSizer(wxVERTICAL, page_general, _("Basic"));
	szr_general->Add(group_basic, 0, wxEXPAND | wxALL, 5);
	wxFlexGridSizer* const szr_basic = new wxFlexGridSizer(2, 5, 5);
	group_basic->Add(szr_basic, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// graphics api
	//{
	//const wxString gfxapi_choices[] = { _("Software [not present]"),
	//	_("OpenGL [broken]"), _("Direct3D 9 [broken]"), _("Direct3D 11") };

	//szr_basic->Add(new wxStaticText(page_general, -1, _("Graphics API:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	//wxChoice* const choice_gfxapi = new SettingChoice(page_general,
	//	g_gfxapi, sizeof(gfxapi_choices)/sizeof(*gfxapi_choices), gfxapi_choices);
	//szr_basic->Add(choice_gfxapi, 1, 0, 0);
	// TODO: Connect with Event_Backend()
	//}

	// adapter // for D3D only
	if (vconfig.backend_info.Adapters.size())
	{
	szr_basic->Add(new wxStaticText(page_general, -1, _("Adapter:")), 1, wxALIGN_CENTER_VERTICAL, 5);
	wxChoice* const choice_adapter = new SettingChoice(page_general, vconfig.iAdapter, wxGetTranslation(adapter_tooltip));

	std::vector<std::string>::const_iterator
		it = vconfig.backend_info.Adapters.begin(),
		itend = vconfig.backend_info.Adapters.end();
	for (; it != itend; ++it)
		choice_adapter->AppendString(wxString::FromAscii(it->c_str()));

	choice_adapter->Select(vconfig.iAdapter);

	szr_basic->Add(choice_adapter, 1, 0, 0);
	}
	
	// aspect-ratio
	{
	const wxString ar_choices[] = { _("Auto [recommended]"),
		_("Force 16:9"), _("Force 4:3"), _("Stretch to Window") };

	szr_basic->Add(new wxStaticText(page_general, -1, _("Aspect Ratio:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	wxChoice* const choice_aspect = new SettingChoice(page_general,
		vconfig.iAspectRatio, wxGetTranslation(ar_tooltip), sizeof(ar_choices)/sizeof(*ar_choices), ar_choices);
	szr_basic->Add(choice_aspect, 1, 0, 0);
	}

	// widescreen hack
	{
	szr_basic->AddStretchSpacer(1);
	szr_basic->Add(new SettingCheckBox(page_general, _("Widescreen Hack"), wxGetTranslation(ws_hack_tooltip), vconfig.bWidescreenHack), 1, 0, 0);
	szr_basic->AddStretchSpacer(1);
	szr_basic->Add(new SettingCheckBox(page_general, _("V-Sync"), wxGetTranslation(vsync_tooltip), vconfig.bVSync), 1, 0, 0);
	}

	// enhancements
	{
	wxStaticBoxSizer* const group_enh = new wxStaticBoxSizer(wxVERTICAL, page_general, _("Enhancements"));
	szr_general->Add(group_enh, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxFlexGridSizer* const szr_enh = new wxFlexGridSizer(2, 5, 5);
	group_enh->Add(szr_enh, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_enh->Add(new wxStaticText(page_general, -1, _("Anisotropic Filtering:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	const wxString af_choices[] = {wxT("1x"), wxT("2x"), wxT("4x"), wxT("8x"), wxT("16x")};
	szr_enh->Add(new SettingChoice(page_general, vconfig.iMaxAnisotropy, wxGetTranslation(af_tooltip), 5, af_choices));


	wxStaticText* const text_aamode = new wxStaticText(page_general, -1, _("Anti-Aliasing:"));
	szr_enh->Add(text_aamode, 1, wxALIGN_CENTER_VERTICAL, 0);
	SettingChoice* const choice_aamode = new SettingChoice(page_general, vconfig.iMultisampleMode, wxGetTranslation(aa_tooltip));

	std::vector<std::string>::const_iterator
		it = vconfig.backend_info.AAModes.begin(),
		itend = vconfig.backend_info.AAModes.end();
	for (; it != itend; ++it)
		choice_aamode->AppendString(wxGetTranslation(wxString::FromAscii(it->c_str())));

	if (vconfig.backend_info.AAModes.size() <= 1)
	{
		choice_aamode->Disable();
		text_aamode->Disable();
	}

	choice_aamode->Select(vconfig.iMultisampleMode);
	szr_enh->Add(choice_aamode);


	szr_enh->Add(new SettingCheckBox(page_general, _("Load Native Mipmaps"), wxGetTranslation(native_mips_tooltip), vconfig.bUseNativeMips));
	szr_enh->Add(new SettingCheckBox(page_general, _("EFB Scaled Copy"), wxGetTranslation(scaled_efb_copy_tooltip), vconfig.bCopyEFBScaled));	
	szr_enh->Add(new SettingCheckBox(page_general, _("Pixel Lighting"), wxGetTranslation(pixel_lighting_tooltip), vconfig.bEnablePixelLigting));
	szr_enh->Add(new SettingCheckBox(page_general, _("Pixel Depth"), wxGetTranslation(pixel_depth_tooltip), vconfig.bEnablePerPixelDepth));
	szr_enh->Add(new SettingCheckBox(page_general, _("Force Bi/Trilinear Filtering"), wxGetTranslation(force_filtering_tooltip), vconfig.bForceFiltering));
	
	if (vconfig.backend_info.bSupports3DVision)
	{
		szr_enh->Add(new SettingCheckBox(page_general, _("3D Vision (Requires Fullscreen)"), wxGetTranslation(_3d_vision_tooltip), vconfig.b3DVision));
	}

	}

	// - EFB
	{
	wxStaticBoxSizer* const group_efb = new wxStaticBoxSizer(wxVERTICAL, page_general, _("EFB"));
	szr_general->Add(group_efb, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// EFB scale
	{
	wxBoxSizer* const efb_scale_szr = new wxBoxSizer(wxHORIZONTAL);
	// TODO: give this a label (?)
	const wxString efbscale_choices[] = { _("Fractional"), _("Integral [recommended]"),
		wxT("1x"), wxT("2x"), wxT("3x"), wxT("0.75x"), wxT("0.5x"), wxT("0.375x") };

	wxChoice *const choice_efbscale = new SettingChoice(page_general,
		vconfig.iEFBScale, wxGetTranslation(internal_res_tooltip), sizeof(efbscale_choices)/sizeof(*efbscale_choices), efbscale_choices);

	efb_scale_szr->Add(new wxStaticText(page_general, -1, _("Scale:")), 0, wxALIGN_CENTER_VERTICAL, 5);
	//efb_scale_szr->AddStretchSpacer(1);
	efb_scale_szr->Add(choice_efbscale, 0, wxBOTTOM | wxLEFT, 5);

	group_efb->Add(efb_scale_szr, 0, wxBOTTOM | wxLEFT, 5);
	}

	group_efb->Add(new SettingCheckBox(page_general, _("Enable CPU Access"), wxGetTranslation(efb_access_tooltip), vconfig.bEFBAccessEnable), 0, wxBOTTOM | wxLEFT, 5);
	SettingCheckBox *emulate_efb_format_changes = new SettingCheckBox(page_general, _("Emulate format changes"), wxGetTranslation(efb_emulate_format_changes_tooltip), vconfig.bEFBEmulateFormatChanges);
	group_efb->Add(emulate_efb_format_changes, 0, wxBOTTOM | wxLEFT, 5);

	if (!vconfig.backend_info.bSupportsFormatReinterpretation)
	{
		emulate_efb_format_changes->SetValue(false);
		emulate_efb_format_changes->Disable();
	}

	// EFB copy
	wxStaticBoxSizer* const group_efbcopy = new wxStaticBoxSizer(wxHORIZONTAL, page_general, _("Copy"));
	group_efb->Add(group_efbcopy, 0, wxEXPAND | wxBOTTOM, 5);

	SettingCheckBox* efbcopy_enable = new SettingCheckBox(page_general, _("Enable"), wxGetTranslation(efb_copy_tooltip), vconfig.bEFBCopyEnable);
	_connect_macro_(efbcopy_enable, VideoConfigDiag::Event_EfbCopy, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	efbcopy_texture = new SettingRadioButton(page_general, _("Texture"), wxGetTranslation(efb_copy_texture_tooltip), vconfig.bCopyEFBToTexture, false, wxRB_GROUP);
	_connect_macro_(efbcopy_texture, VideoConfigDiag::Event_EfbCopyToTexture, wxEVT_COMMAND_RADIOBUTTON_SELECTED, this);
	efbcopy_ram = new SettingRadioButton(page_general, _("RAM"), wxGetTranslation(efb_copy_ram_tooltip), vconfig.bCopyEFBToTexture, true);
	_connect_macro_(efbcopy_ram, VideoConfigDiag::Event_EfbCopyToRam, wxEVT_COMMAND_RADIOBUTTON_SELECTED, this);
	cache_efb_copies = new SettingCheckBox(page_general, _("Enable cache"), wxGetTranslation(cache_efb_copies_tooltip), vconfig.bEFBCopyCacheEnable);
	group_efbcopy->Add(efbcopy_enable, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_efbcopy->AddStretchSpacer(1);
	group_efbcopy->Add(efbcopy_texture, 0, wxRIGHT, 5);
	group_efbcopy->Add(efbcopy_ram, 0, wxRIGHT, 5);
	group_efbcopy->Add(cache_efb_copies, 0, wxRIGHT, 5);

	if (!vconfig.backend_info.bSupportsEFBToRAM)
	{
		efbcopy_ram->Disable();
		vconfig.bCopyEFBToTexture = true;
		efbcopy_texture->SetValue(true);
	}
	if (!vconfig.bEFBCopyEnable)
	{
		efbcopy_ram->Disable();
		efbcopy_texture->Disable();
		cache_efb_copies->Disable();
	}
	else if (vconfig.bCopyEFBToTexture)
		cache_efb_copies->Disable();

	}

	// - safe texture cache
	{
	wxStaticBoxSizer* const group_safetex = new wxStaticBoxSizer(wxHORIZONTAL, page_general, _("Accurate Texture Cache"));
	szr_general->Add(group_safetex, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	SettingCheckBox* stc_enable = new SettingCheckBox(page_general, _("Enable"), wxGetTranslation(stc_tooltip), vconfig.bSafeTextureCache);
	_connect_macro_(stc_enable, VideoConfigDiag::Event_Stc, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	group_safetex->Add(stc_enable, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_safetex->AddStretchSpacer(1);

	stc_safe = new wxRadioButton(page_general, -1, _("Safe"),
		wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	stc_safe->SetToolTip(wxGetTranslation(stc_speed_tooltip));
	_connect_macro_(stc_safe, VideoConfigDiag::Event_StcSafe, wxEVT_COMMAND_RADIOBUTTON_SELECTED, this);
	group_safetex->Add(stc_safe, 0, wxRIGHT, 5);

	stc_normal = new wxRadioButton(page_general, -1, _("Normal"));
	stc_normal->SetToolTip(wxGetTranslation(stc_speed_tooltip));
	_connect_macro_(stc_normal, VideoConfigDiag::Event_StcNormal, wxEVT_COMMAND_RADIOBUTTON_SELECTED, this);
	group_safetex->Add(stc_normal, 0, wxRIGHT, 5);

	stc_fast = new wxRadioButton(page_general, -1, _("Fast"));
	stc_fast->SetToolTip(wxGetTranslation(stc_speed_tooltip));
	_connect_macro_(stc_fast, VideoConfigDiag::Event_StcFast, wxEVT_COMMAND_RADIOBUTTON_SELECTED, this);
	group_safetex->Add(stc_fast, 0, wxRIGHT, 5);

	if (0 == vconfig.iSafeTextureCache_ColorSamples)
		stc_safe->SetValue(true);

	if (512 == vconfig.iSafeTextureCache_ColorSamples)
		stc_normal->SetValue(true);

	if (128 == vconfig.iSafeTextureCache_ColorSamples)
		stc_fast->SetValue(true);

	if (!vconfig.bSafeTextureCache)
	{
		stc_safe->Disable();
		stc_normal->Disable();
		stc_fast->Disable();
	}

	}

	}

	page_general->SetSizerAndFit(szr_general);
	}

	// -- ADVANCED --
	{
	wxPanel* const page_advanced = new wxPanel(notebook, -1, wxDefaultPosition);
	notebook->AddPage(page_advanced, _("Advanced"));
	wxBoxSizer* const szr_advanced = new wxBoxSizer(wxVERTICAL);

	// - rendering
	{
	wxStaticBoxSizer* const group_rendering = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Rendering"));
	szr_advanced->Add(group_rendering, 0, wxEXPAND | wxALL, 5);
	wxGridSizer* const szr_rendering = new wxGridSizer(2, 5, 5);
	group_rendering->Add(szr_rendering, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_rendering->Add(new SettingCheckBox(page_advanced, _("Enable Wireframe"), wxGetTranslation(wireframe_tooltip), vconfig.bWireFrame));
	szr_rendering->Add(new SettingCheckBox(page_advanced, _("Disable Lighting"), wxGetTranslation(disable_lighting_tooltip), vconfig.bDisableLighting));
	szr_rendering->Add(new SettingCheckBox(page_advanced, _("Disable Textures"), wxGetTranslation(disable_textures_tooltip), vconfig.bDisableTexturing));
	szr_rendering->Add(new SettingCheckBox(page_advanced, _("Disable Fog"), wxGetTranslation(disable_fog_tooltip), vconfig.bDisableFog));
	szr_rendering->Add(new SettingCheckBox(page_advanced, _("Disable Dest. Alpha Pass"), wxGetTranslation(disable_alphapass_tooltip), vconfig.bDstAlphaPass));
	}

	// - info
	{
	wxStaticBoxSizer* const group_info = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Information"));
	szr_advanced->Add(group_info, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxGridSizer* const szr_info = new wxGridSizer(2, 5, 5);
	group_info->Add(szr_info, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_info->Add(new SettingCheckBox(page_advanced, _("Show FPS"), wxGetTranslation(show_fps_tooltip), vconfig.bShowFPS));
	szr_info->Add(new SettingCheckBox(page_advanced, _("Various Statistics"), wxGetTranslation(show_stats_tooltip), vconfig.bOverlayStats));
	szr_info->Add(new SettingCheckBox(page_advanced, _("Projection Stats"), wxGetTranslation(proj_stats_tooltip), vconfig.bOverlayProjStats));
	szr_info->Add(new SettingCheckBox(page_advanced, _("Texture Format"), wxGetTranslation(texfmt_tooltip), vconfig.bTexFmtOverlayEnable));
	szr_info->Add(new SettingCheckBox(page_advanced, _("EFB Copy Regions"), wxGetTranslation(efb_copy_regions_tooltip), vconfig.bShowEFBCopyRegions));
	szr_info->Add(new SettingCheckBox(page_advanced, _("Show Shader Errors"), wxT(""), vconfig.bShowShaderErrors));
	}
	
	// - XFB
	{
	wxStaticBoxSizer* const group_xfb = new wxStaticBoxSizer(wxHORIZONTAL, page_advanced, _("XFB"));
	szr_advanced->Add(group_xfb, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	SettingCheckBox* enable_xfb = new SettingCheckBox(page_advanced, _("Enable"), wxGetTranslation(xfb_tooltip), vconfig.bUseXFB);
	_connect_macro_(enable_xfb, VideoConfigDiag::Event_Xfb, wxEVT_COMMAND_CHECKBOX_CLICKED, this);
	virtual_xfb = new SettingRadioButton(page_advanced, _("Virtual"), wxGetTranslation(xfb_tooltip), vconfig.bUseRealXFB, true, wxRB_GROUP);
	real_xfb = new SettingRadioButton(page_advanced, _("Real"), wxGetTranslation(xfb_tooltip), vconfig.bUseRealXFB);
	group_xfb->Add(enable_xfb, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_xfb->AddStretchSpacer(1);
	group_xfb->Add(virtual_xfb, 0, wxRIGHT, 5);
	group_xfb->Add(real_xfb, 0, wxRIGHT, 5);

	if (!vconfig.backend_info.bSupportsRealXFB)
	{
		real_xfb->Disable();
		vconfig.bUseRealXFB = false;
		virtual_xfb->SetValue(true);
	}

	if (!vconfig.bUseXFB)
	{
		real_xfb->Disable();
		virtual_xfb->Disable();
	}

	}	// xfb

	// - utility
	{
	wxStaticBoxSizer* const group_utility = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Utility"));
	szr_advanced->Add(group_utility, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxGridSizer* const szr_utility = new wxGridSizer(2, 5, 5);
	group_utility->Add(szr_utility, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_utility->Add(new SettingCheckBox(page_advanced, _("Dump Textures"), wxGetTranslation(dump_textures_tooltip), vconfig.bDumpTextures));
	szr_utility->Add(new SettingCheckBox(page_advanced, _("Load Hi-Res Textures"), wxGetTranslation(load_hires_textures_tooltip), vconfig.bHiresTextures));
	szr_utility->Add(new SettingCheckBox(page_advanced, _("Dump EFB Target"), dump_efb_tooltip, vconfig.bDumpEFBTarget));
	szr_utility->Add(new SettingCheckBox(page_advanced, _("Dump Frames"), dump_frames_tooltip, vconfig.bDumpFrames));
	szr_utility->Add(new SettingCheckBox(page_advanced, _("Free Look"), free_look_tooltip, vconfig.bFreeLook));
	}

	// - misc
	{
	wxStaticBoxSizer* const group_misc = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Misc"));
	szr_advanced->Add(group_misc, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	wxFlexGridSizer* const szr_misc = new wxFlexGridSizer(2, 5, 5);
	group_misc->Add(szr_misc, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	szr_misc->Add(new SettingCheckBox(page_advanced, _("Crop"), crop_tooltip, vconfig.bCrop));
	szr_misc->Add(new SettingCheckBox(page_advanced, _("Enable OpenCL"), opencl_tooltip, vconfig.bEnableOpenCL));
	szr_misc->Add(new SettingCheckBox(page_advanced, _("Enable Display List Caching"), dlc_tooltip, vconfig.bDlistCachingEnable));
	szr_misc->Add(new SettingCheckBox(page_advanced, _("Enable Hotkeys"), hotkeys_tooltip, vconfig.bOSDHotKey));
	szr_misc->Add(new SettingCheckBox(page_advanced, _("Adjust window size"), wxGetTranslation(adjust_window_size_tooltip), vconfig.bAdjustWindowSize));
	szr_misc->AddSpacer(0);

	// postproc shader
	if (vconfig.backend_info.PPShaders.size())
	{
		szr_misc->Add(new wxStaticText(page_advanced, -1, _("Post-Processing Shader:")), 1, wxALIGN_CENTER_VERTICAL, 0);

		wxChoice *const choice_ppshader = new wxChoice(page_advanced, -1, wxDefaultPosition);
		choice_ppshader->SetToolTip(wxGetTranslation(ppshader_tooltip));
		choice_ppshader->AppendString(_("(off)"));

		std::vector<std::string>::const_iterator
			it = vconfig.backend_info.PPShaders.begin(),
			itend = vconfig.backend_info.PPShaders.end();
		for (; it != itend; ++it)
			choice_ppshader->AppendString(wxString::FromAscii(it->c_str()));

		if (vconfig.sPostProcessingShader.empty())
			choice_ppshader->Select(0);
		else
			choice_ppshader->SetStringSelection(wxString::FromAscii(vconfig.sPostProcessingShader.c_str()));

		_connect_macro_(choice_ppshader, VideoConfigDiag::Event_PPShader, wxEVT_COMMAND_CHOICE_SELECTED, this);

		szr_misc->Add(choice_ppshader, 0, wxLEFT, 5);
	}

	}

	page_advanced->SetSizerAndFit(szr_advanced);
	}

	wxButton* const btn_close = new wxButton(this, -1, _("Close"), wxDefaultPosition);
	_connect_macro_(btn_close, VideoConfigDiag::Event_ClickClose, wxEVT_COMMAND_BUTTON_CLICKED, this);

	Connect(-1, wxEVT_CLOSE_WINDOW, wxCloseEventHandler(VideoConfigDiag::Event_Close), (wxObject*)0, this);

	wxBoxSizer* const szr_main = new wxBoxSizer(wxVERTICAL);
	szr_main->Add(notebook, 1, wxEXPAND | wxALL, 5);
	szr_main->Add(btn_close, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 5);

	SetSizerAndFit(szr_main);
	Center();
}
