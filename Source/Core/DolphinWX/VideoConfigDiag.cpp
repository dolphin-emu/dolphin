// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/control.h>
#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/stattext.h>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/SysConf.h"
#include "Core/ConfigManager.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/VideoConfigDiag.h"
#include "DolphinWX/WxUtils.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

// template instantiation
template class BoolSetting<wxCheckBox>;
template class BoolSetting<wxRadioButton>;

template <>
SettingCheckBox::BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse, long style)
	: wxCheckBox(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, style)
	, m_setting(setting)
	, m_reverse(reverse)
{
	SetToolTip(tooltip);
	SetValue(m_setting ^ m_reverse);
	Bind(wxEVT_CHECKBOX, &SettingCheckBox::UpdateValue, this);
}

template <>
SettingRadioButton::BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip, bool &setting, bool reverse, long style)
	: wxRadioButton(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, style)
	, m_setting(setting)
	, m_reverse(reverse)
{
	SetToolTip(tooltip);
	SetValue(m_setting ^ m_reverse);
	Bind(wxEVT_RADIOBUTTON, &SettingRadioButton::UpdateValue, this);
}

SettingChoice::SettingChoice(wxWindow* parent, int &setting, const wxString& tooltip, int num, const wxString choices[], long style)
	: wxChoice(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, num, choices)
	, m_setting(setting)
{
	SetToolTip(tooltip);
	Select(m_setting);
	Bind(wxEVT_CHOICE, &SettingChoice::UpdateValue, this);
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
	g_Config.Save(File::GetUserPath(D_CONFIG_IDX) + "GFX.ini");

	EndModal(wxID_OK);
}

#if defined(_WIN32)
static wxString backend_desc = wxTRANSLATE("Selects what graphics API to use internally.\nThe software renderer is extremely slow and only useful for debugging, so you'll want to use either Direct3D or OpenGL. Different games and different GPUs will behave differently on each backend, so for the best emulation experience it's recommended to try both and choose the one that's less problematic.\n\nIf unsure, select OpenGL.");
#else
static wxString backend_desc = wxTRANSLATE("Selects what graphics API to use internally.\nThe software renderer is extremely slow and only useful for debugging, so unless you have a reason to use it you'll want to select OpenGL here.\n\nIf unsure, select OpenGL.");
#endif
static wxString adapter_desc = wxTRANSLATE("Selects a hardware adapter to use.\n\nIf unsure, use the first one.");
static wxString display_res_desc = wxTRANSLATE("Selects the display resolution used in fullscreen mode.\nThis should always be bigger than or equal to the internal resolution. Performance impact is negligible.\n\nIf unsure, select auto.");
static wxString use_fullscreen_desc = wxTRANSLATE("Enable this if you want the whole screen to be used for rendering.\nIf this is disabled, a render window will be created instead.\n\nIf unsure, leave this unchecked.");
static wxString auto_window_size_desc = wxTRANSLATE("Automatically adjusts the window size to your internal resolution.\n\nIf unsure, leave this unchecked.");
static wxString keep_window_on_top_desc = wxTRANSLATE("Keep the game window on top of all other windows.\n\nIf unsure, leave this unchecked.");
static wxString hide_mouse_cursor_desc = wxTRANSLATE("Hides the mouse cursor if it's on top of the emulation window.\n\nIf unsure, leave this unchecked.");
static wxString render_to_main_win_desc = wxTRANSLATE("Enable this if you want to use the main Dolphin window for rendering rather than a separate render window.\n\nIf unsure, leave this unchecked.");
static wxString prog_scan_desc = wxTRANSLATE("Enables progressive scan if supported by the emulated software.\nMost games don't care about this.\n\nIf unsure, leave this unchecked.");
static wxString ar_desc = wxTRANSLATE("Select what aspect ratio to use when rendering:\nAuto: Use the native aspect ratio\nForce 16:9: Mimic an analog TV with a widescreen aspect ratio.\nForce 4:3: Mimic a standard 4:3 analog TV.\nStretch to Window: Stretch the picture to the window size.\n\nIf unsure, select Auto.");
static wxString ws_hack_desc = wxTRANSLATE("Forces the game to output graphics for any aspect ratio.\nUse with \"Aspect Ratio\" set to \"Force 16:9\" to force 4:3-only games to run at 16:9.\nRarely produces good results and often partially breaks graphics and game UIs.\nUnnecessary (and detrimental) if using any AR/Gecko-code widescreen patches.\n\nIf unsure, leave this unchecked.");
static wxString vsync_desc = wxTRANSLATE("Wait for vertical blanks in order to reduce tearing.\nDecreases performance if emulation speed is below 100%.\n\nIf unsure, leave this unchecked.");
static wxString af_desc = wxTRANSLATE("Enable anisotropic filtering.\nEnhances visual quality of textures that are at oblique viewing angles.\nMight cause issues in a small number of games.\nOn Direct3D, setting this above 1x will also have the same effect as enabling \"Force Texture Filtering\".\n\nIf unsure, select 1x.");
static wxString aa_desc = wxTRANSLATE("Reduces the amount of aliasing caused by rasterizing 3D graphics. This smooths out jagged edges on objects.\nIncreases GPU load and sometimes causes graphical issues. SSAA is significantly more demanding than MSAA, but provides top quality geometry anti-aliasing and also applies anti-aliasing to lighting, shader effects, and textures.\n\nIf unsure, select None.");
static wxString scaled_efb_copy_desc = wxTRANSLATE("Greatly increases quality of textures generated using render-to-texture effects.\nRaising the internal resolution will improve the effect of this setting.\nSlightly increases GPU load and causes relatively few graphical issues.\n\nIf unsure, leave this checked.");
static wxString pixel_lighting_desc = wxTRANSLATE("Calculates lighting of 3D objects per-pixel rather than per-vertex, smoothing out the appearance of lit polygons and making individual triangles less noticeable.\nRarely causes slowdowns or graphical issues.\n\nIf unsure, leave this unchecked.");
static wxString fast_depth_calc_desc = wxTRANSLATE("Use a less accurate algorithm to calculate depth values.\nCauses issues in a few games, but can give a decent speedup depending on the game and/or your GPU.\n\nIf unsure, leave this checked.");
static wxString disable_bbox_desc = wxTRANSLATE("Disable the bounding box emulation.\nThis may improve the GPU performance a lot, but some games will break.\n\nIf unsure, leave this checked.");
static wxString force_filtering_desc = wxTRANSLATE("Filter all textures, including any that the game explicitly set as unfiltered.\nMay improve quality of certain textures in some games, but will cause issues in others.\nOn Direct3D, setting Anisotropic Filtering above 1x will also have the same effect as enabling this option.\n\nIf unsure, leave this unchecked.");
static wxString borderless_fullscreen_desc = wxTRANSLATE("Implement fullscreen mode with a borderless window spanning the whole screen instead of using exclusive mode.\nAllows for faster transitions between fullscreen and windowed mode, but slightly increases input latency, makes movement less smooth and slightly decreases performance.\nExclusive mode is required for Nvidia 3D Vision to work in the Direct3D backend.\n\nIf unsure, leave this unchecked.");
static wxString internal_res_desc = wxTRANSLATE("Specifies the resolution used to render at. A high resolution greatly improves visual quality, but also greatly increases GPU load and can cause issues in certain games.\n\"Multiple of 640x528\" will result in a size slightly larger than \"Window Size\" but yield fewer issues. Generally speaking, the lower the internal resolution is, the better your performance will be. Auto (Window Size), 1.5x, and 2.5x may cause issues in some games.\n\nIf unsure, select Native.");
static wxString efb_access_desc = wxTRANSLATE("Ignore any requests from the CPU to read from or write to the EFB.\nImproves performance in some games, but might disable some gameplay-related features or graphical effects.\n\nIf unsure, leave this unchecked.");
static wxString efb_emulate_format_changes_desc = wxTRANSLATE("Ignore any changes to the EFB format.\nImproves performance in many games without any negative effect. Causes graphical defects in a small number of other games.\n\nIf unsure, leave this checked.");
static wxString skip_efb_copy_to_ram_desc = wxTRANSLATE("Stores EFB Copies exclusively on the GPU, bypassing system memory. Causes graphical defects in a small number of games.\n\nEnabled = EFB Copies to Texture\nDisabled = EFB Copies to RAM (and Texture)\n\nIf unsure, leave this checked.");
static wxString stc_desc = wxTRANSLATE("The \"Safe\" setting eliminates the likelihood of the GPU missing texture updates from RAM.\nLower accuracies cause in-game text to appear garbled in certain games.\n\nIf unsure, use the rightmost value.");
static wxString wireframe_desc = wxTRANSLATE("Render the scene as a wireframe.\n\nIf unsure, leave this unchecked.");
static wxString disable_fog_desc = wxTRANSLATE("Makes distant objects more visible by removing fog, thus increasing the overall detail.\nDisabling fog will break some games which rely on proper fog emulation.\n\nIf unsure, leave this unchecked.");
static wxString show_fps_desc = wxTRANSLATE("Show the number of frames rendered per second as a measure of emulation speed.\n\nIf unsure, leave this unchecked.");
static wxString log_render_time_to_file_desc = wxTRANSLATE("Log the render time of every frame to User/Logs/render_time.txt. Use this feature when you want to measure the performance of Dolphin.\n\nIf unsure, leave this unchecked.");
static wxString show_stats_desc = wxTRANSLATE("Show various rendering statistics.\n\nIf unsure, leave this unchecked.");
static wxString texfmt_desc = wxTRANSLATE("Modify textures to show the format they're encoded in. Needs an emulation reset in most cases.\n\nIf unsure, leave this unchecked.");
static wxString xfb_desc = wxTRANSLATE("Disable any XFB emulation.\nSpeeds up emulation a lot but causes heavy glitches in many games which rely on them (especially homebrew applications).\n\nIf unsure, leave this checked.");
static wxString xfb_virtual_desc = wxTRANSLATE("Emulate XFBs using GPU texture objects.\nFixes many games which don't work without XFB emulation while not being as slow as real XFB emulation. However, it may still fail for a lot of other games (especially homebrew applications).\n\nIf unsure, leave this checked.");
static wxString xfb_real_desc = wxTRANSLATE("Emulate XFBs accurately.\nSlows down emulation a lot and prohibits high-resolution rendering but is necessary to emulate a number of games properly.\n\nIf unsure, check virtual XFB emulation instead.");
static wxString dump_textures_desc = wxTRANSLATE("Dump decoded game textures to User/Dump/Textures/<game_id>/.\n\nIf unsure, leave this unchecked.");
static wxString load_hires_textures_desc = wxTRANSLATE("Load custom textures from User/Load/Textures/<game_id>/.\n\nIf unsure, leave this unchecked.");
static wxString cache_hires_textures_desc = wxTRANSLATE("Cache custom textures to system RAM on startup.\nThis can require exponentially more RAM but fixes possible stuttering.\n\nIf unsure, leave this unchecked.");
static wxString dump_efb_desc = wxTRANSLATE("Dump the contents of EFB copies to User/Dump/Textures/.\n\nIf unsure, leave this unchecked.");
#if defined(HAVE_LIBAV) || defined (_WIN32)
static wxString use_ffv1_desc = wxTRANSLATE("Encode frame dumps using the FFV1 codec.\n\nIf unsure, leave this unchecked.");
#endif
static wxString free_look_desc = wxTRANSLATE("This feature allows you to change the game's camera.\nMove the mouse while holding the right mouse button to pan and while holding the middle button to move.\nHold SHIFT and press one of the WASD keys to move the camera by a certain step distance (SHIFT+2 to move faster and SHIFT+1 to move slower). Press SHIFT+R to reset the camera and SHIFT+F to reset the speed.\n\nIf unsure, leave this unchecked.");
static wxString crop_desc = wxTRANSLATE("Crop the picture from its native aspect ratio to 4:3 or 16:9.\n\nIf unsure, leave this unchecked.");
static wxString ppshader_desc = wxTRANSLATE("Apply a post-processing effect after finishing a frame.\n\nIf unsure, select (off).");
static wxString cache_efb_copies_desc = wxTRANSLATE("Slightly speeds up EFB to RAM copies by sacrificing emulation accuracy.\nIf you're experiencing any issues, try raising texture cache accuracy or disable this option.\n\nIf unsure, leave this unchecked.");
static wxString stereo_3d_desc = wxTRANSLATE("Selects the stereoscopic 3D mode. Stereoscopy allows you to get a better feeling of depth if you have the necessary hardware.\nSide-by-Side and Top-and-Bottom are used by most 3D TVs.\nAnaglyph is used for Red-Cyan colored glasses.\nHeavily decreases emulation speed and sometimes causes issues.\n\nIf unsure, select Off.");
static wxString stereo_depth_desc = wxTRANSLATE("Controls the separation distance between the virtual cameras.\nA higher value creates a stronger feeling of depth while a lower value is more comfortable.");
static wxString stereo_convergence_desc = wxTRANSLATE("Controls the distance of the convergence plane. This is the distance at which virtual objects will appear to be in front of the screen.\nA higher value creates stronger out-of-screen effects while a lower value is more comfortable.");
static wxString stereo_swap_desc = wxTRANSLATE("Swaps the left and right eye. Mostly useful if you want to view side-by-side cross-eyed.\n\nIf unsure, leave this unchecked.");

#if !defined(__APPLE__)
// Search for available resolutions - TODO: Move to Common?
static wxArrayString GetListOfResolutions()
{
	wxArrayString retlist;
	retlist.Add(_("Auto"));
#ifdef _WIN32
	DWORD iModeNum = 0;
	DEVMODE dmi;
	ZeroMemory(&dmi, sizeof(dmi));
	dmi.dmSize = sizeof(dmi);
	std::vector<std::string> resos;

	while (EnumDisplaySettings(nullptr, iModeNum++, &dmi) != 0)
	{
		char res[100];
		sprintf(res, "%dx%d", dmi.dmPelsWidth, dmi.dmPelsHeight);
		std::string strRes(res);
		// Only add unique resolutions
		if (std::find(resos.begin(), resos.end(), strRes) == resos.end())
		{
			resos.push_back(strRes);
			retlist.Add(StrToWxStr(res));
		}
		ZeroMemory(&dmi, sizeof(dmi));
	}
#elif defined(HAVE_XRANDR) && HAVE_XRANDR
	std::vector<std::string> resos;
	main_frame->m_XRRConfig->AddResolutions(resos);
	for (auto res : resos)
		retlist.Add(StrToWxStr(res));
#elif defined(__APPLE__)
	CFArrayRef modes = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), nullptr);
	for (CFIndex i = 0; i < CFArrayGetCount(modes); i++)
	{
		std::stringstream res;
		CGDisplayModeRef mode;
		CFStringRef encoding;
		size_t w, h;
		bool is32;

		mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
		w = CGDisplayModeGetWidth(mode);
		h = CGDisplayModeGetHeight(mode);
		encoding = CGDisplayModeCopyPixelEncoding(mode);
		is32 = CFEqual(encoding, CFSTR(IO32BitDirectPixels));
		CFRelease(encoding);

		if (!is32)
			continue;
		if (CGDisplayModeGetIOFlags(mode) & kDisplayModeStretchedFlag)
			continue;

		res << w << "x" << h;

		retlist.Add(res.str());
	}
	CFRelease(modes);
#endif
	return retlist;
}
#endif

VideoConfigDiag::VideoConfigDiag(wxWindow* parent, const std::string &title, const std::string& ininame)
	: wxDialog(parent, wxID_ANY,
		wxString::Format(_("Dolphin %s Graphics Configuration"), wxGetTranslation(StrToWxStr(title))))
	, vconfig(g_Config)
{
	if (File::Exists(File::GetUserPath(D_CONFIG_IDX) + "GFX.ini"))
		vconfig.Load(File::GetUserPath(D_CONFIG_IDX) + "GFX.ini");
	else
		vconfig.Load(File::GetUserPath(D_CONFIG_IDX) + ininame + ".ini");

	Bind(wxEVT_UPDATE_UI, &VideoConfigDiag::OnUpdateUI, this);

	wxNotebook* const notebook = new wxNotebook(this, wxID_ANY);

	// -- GENERAL --
	{
	wxPanel* const page_general = new wxPanel(notebook);
	notebook->AddPage(page_general, _("General"));
	wxBoxSizer* const szr_general = new wxBoxSizer(wxVERTICAL);

	// - basic
	{
	wxFlexGridSizer* const szr_basic = new wxFlexGridSizer(2, 5, 5);

	// backend
	{
	label_backend = new wxStaticText(page_general, wxID_ANY, _("Backend:"));
	choice_backend = new wxChoice(page_general, wxID_ANY);
	RegisterControl(choice_backend, wxGetTranslation(backend_desc));

	for (const auto& backend : g_available_video_backends)
	{
		choice_backend->AppendString(wxGetTranslation(StrToWxStr(backend->GetDisplayName())));
	}

	choice_backend->SetStringSelection(wxGetTranslation(StrToWxStr(g_video_backend->GetDisplayName())));
	choice_backend->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_Backend, this);

	szr_basic->Add(label_backend, 1, wxALIGN_CENTER_VERTICAL, 5);
	szr_basic->Add(choice_backend, 1, 0, 0);
	}

	// adapter (D3D only)
	if (vconfig.backend_info.Adapters.size())
	{
		choice_adapter = CreateChoice(page_general, vconfig.iAdapter, wxGetTranslation(adapter_desc));

		for (const std::string& adapter : vconfig.backend_info.Adapters)
		{
			choice_adapter->AppendString(StrToWxStr(adapter));
		}

		choice_adapter->Select(vconfig.iAdapter);

		label_adapter = new wxStaticText(page_general, wxID_ANY, _("Adapter:"));
		szr_basic->Add(label_adapter, 1, wxALIGN_CENTER_VERTICAL, 5);
		szr_basic->Add(choice_adapter, 1, 0, 0);
	}


	// - display
	wxFlexGridSizer* const szr_display = new wxFlexGridSizer(2, 5, 5);

	{

#if !defined(__APPLE__)
	// display resolution
	{
		wxArrayString res_list = GetListOfResolutions();
		if (res_list.empty())
			res_list.Add(_("<No resolutions found>"));
		label_display_resolution = new wxStaticText(page_general, wxID_ANY, _("Fullscreen Resolution:"));
		choice_display_resolution = new wxChoice(page_general, wxID_ANY, wxDefaultPosition, wxDefaultSize, res_list);
		RegisterControl(choice_display_resolution, wxGetTranslation(display_res_desc));
		choice_display_resolution->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_DisplayResolution, this);

		choice_display_resolution->SetStringSelection(StrToWxStr(SConfig::GetInstance().strFullscreenResolution));

		szr_display->Add(label_display_resolution, 1, wxALIGN_CENTER_VERTICAL, 0);
		szr_display->Add(choice_display_resolution);
	}
#endif

	// aspect-ratio
	{
	const wxString ar_choices[] = { _("Auto"), _("Force 16:9"), _("Force 4:3"), _("Stretch to Window") };

	szr_display->Add(new wxStaticText(page_general, wxID_ANY, _("Aspect Ratio:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	wxChoice* const choice_aspect = CreateChoice(page_general, vconfig.iAspectRatio, wxGetTranslation(ar_desc),
	                                             sizeof(ar_choices)/sizeof(*ar_choices), ar_choices);
	szr_display->Add(choice_aspect, 1, 0, 0);
	}

	// various other display options
	{
	szr_display->Add(CreateCheckBox(page_general, _("V-Sync"), wxGetTranslation(vsync_desc), vconfig.bVSync));
	szr_display->Add(CreateCheckBox(page_general, _("Use Fullscreen"), wxGetTranslation(use_fullscreen_desc), SConfig::GetInstance().bFullscreen));
	}
	}

	// - other
	wxFlexGridSizer* const szr_other = new wxFlexGridSizer(2, 5, 5);

	{
	szr_other->Add(CreateCheckBox(page_general, _("Show FPS"), wxGetTranslation(show_fps_desc), vconfig.bShowFPS));
	szr_other->Add(CreateCheckBox(page_general, _("Log Render Time to File"), wxGetTranslation(log_render_time_to_file_desc), vconfig.bLogRenderTimeToFile));
	szr_other->Add(CreateCheckBox(page_general, _("Auto adjust Window Size"), wxGetTranslation(auto_window_size_desc), SConfig::GetInstance().bRenderWindowAutoSize));
	szr_other->Add(CreateCheckBox(page_general, _("Keep Window on Top"), wxGetTranslation(keep_window_on_top_desc), SConfig::GetInstance().bKeepWindowOnTop));
	szr_other->Add(CreateCheckBox(page_general, _("Hide Mouse Cursor"), wxGetTranslation(hide_mouse_cursor_desc), SConfig::GetInstance().bHideCursor));
	szr_other->Add(render_to_main_checkbox = CreateCheckBox(page_general, _("Render to Main Window"), wxGetTranslation(render_to_main_win_desc), SConfig::GetInstance().bRenderToMain));
	}


	wxStaticBoxSizer* const group_basic = new wxStaticBoxSizer(wxVERTICAL, page_general, _("Basic"));
	group_basic->Add(szr_basic, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	szr_general->Add(group_basic, 0, wxEXPAND | wxALL, 5);

	wxStaticBoxSizer* const group_display = new wxStaticBoxSizer(wxVERTICAL, page_general, _("Display"));
	group_display->Add(szr_display, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	szr_general->Add(group_display, 0, wxEXPAND | wxALL, 5);

	wxStaticBoxSizer* const group_other = new wxStaticBoxSizer(wxVERTICAL, page_general, _("Other"));
	group_other->Add(szr_other, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	szr_general->Add(group_other, 0, wxEXPAND | wxALL, 5);
	}

	szr_general->AddStretchSpacer();
	CreateDescriptionArea(page_general, szr_general);
	page_general->SetSizerAndFit(szr_general);
	}

	// -- ENHANCEMENTS --
	{
	wxPanel* const page_enh = new wxPanel(notebook);
	notebook->AddPage(page_enh, _("Enhancements"));
	wxBoxSizer* const szr_enh_main = new wxBoxSizer(wxVERTICAL);

	// - enhancements
	wxFlexGridSizer* const szr_enh = new wxFlexGridSizer(2, 5, 5);

	// Internal resolution
	{
	const wxString efbscale_choices[] = { _("Auto (Window Size)"), _("Auto (Multiple of 640x528)"),
		_("Native (640x528)"), _("1.5x Native (960x792)"), _("2x Native (1280x1056) for 720p"), _("2.5x Native (1600x1320)"),
		_("3x Native (1920x1584) for 1080p"), _("4x Native (2560x2112) for 1440p"), _("5x Native (3200x2640)"),
		_("6x Native (3840x3168) for 4K"), _("7x Native (4480x3696)"), _("8x Native (5120x4224) for 5K"), _("Custom") };

	wxChoice *const choice_efbscale = CreateChoice(page_enh,
		vconfig.iEFBScale, wxGetTranslation(internal_res_desc), (vconfig.iEFBScale > 11) ?
		ArraySize(efbscale_choices) : ArraySize(efbscale_choices) - 1, efbscale_choices);


	if (vconfig.iEFBScale > 11)
		choice_efbscale->SetSelection(12);

	szr_enh->Add(new wxStaticText(page_enh, wxID_ANY, _("Internal Resolution:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	szr_enh->Add(choice_efbscale);
	}

	// AA
	{

	text_aamode = new wxStaticText(page_enh, wxID_ANY, _("Anti-Aliasing:"));
	choice_aamode = new wxChoice(page_enh, wxID_ANY);
	RegisterControl(choice_aamode, wxGetTranslation(aa_desc));
	PopulateAAList();
	choice_aamode->Bind(wxEVT_CHOICE, &VideoConfigDiag::OnAAChanged, this);

	szr_enh->Add(text_aamode, 1, wxALIGN_CENTER_VERTICAL, 0);
	szr_enh->Add(choice_aamode);

	}

	// AF
	{
	const wxString af_choices[] = {"1x", "2x", "4x", "8x", "16x"};
	szr_enh->Add(new wxStaticText(page_enh, wxID_ANY, _("Anisotropic Filtering:")), 1, wxALIGN_CENTER_VERTICAL, 0);
	szr_enh->Add(CreateChoice(page_enh, vconfig.iMaxAnisotropy, wxGetTranslation(af_desc), 5, af_choices));
	}

	// postproc shader
	if (vconfig.backend_info.bSupportsPostProcessing)
	{
		wxFlexGridSizer* const szr_pp = new wxFlexGridSizer(3, 5, 5);
		choice_ppshader = new wxChoice(page_enh, wxID_ANY);
		RegisterControl(choice_ppshader, wxGetTranslation(ppshader_desc));
		button_config_pp = new wxButton(page_enh, wxID_ANY, _("Config"));

		PopulatePostProcessingShaders();

		choice_ppshader->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_PPShader, this);
		button_config_pp->Bind(wxEVT_BUTTON, &VideoConfigDiag::Event_ConfigurePPShader, this);

		szr_enh->Add(new wxStaticText(page_enh, wxID_ANY, _("Post-Processing Effect:")), 1, wxALIGN_CENTER_VERTICAL, 0);
		szr_pp->Add(choice_ppshader);
		szr_pp->Add(button_config_pp);
		szr_enh->Add(szr_pp);
	}
	else
	{
		choice_ppshader = nullptr;
		button_config_pp = nullptr;
	}

	// Scaled copy, PL, Bilinear filter
	szr_enh->Add(CreateCheckBox(page_enh, _("Scaled EFB Copy"), wxGetTranslation(scaled_efb_copy_desc), vconfig.bCopyEFBScaled));
	szr_enh->Add(CreateCheckBox(page_enh, _("Per-Pixel Lighting"), wxGetTranslation(pixel_lighting_desc), vconfig.bEnablePixelLighting));
	szr_enh->Add(CreateCheckBox(page_enh, _("Force Texture Filtering"), wxGetTranslation(force_filtering_desc), vconfig.bForceFiltering));
	szr_enh->Add(CreateCheckBox(page_enh, _("Widescreen Hack"), wxGetTranslation(ws_hack_desc), vconfig.bWidescreenHack));
	szr_enh->Add(CreateCheckBox(page_enh, _("Disable Fog"), wxGetTranslation(disable_fog_desc), vconfig.bDisableFog));

	wxStaticBoxSizer* const group_enh = new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Enhancements"));
	group_enh->Add(szr_enh, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	szr_enh_main->Add(group_enh, 0, wxEXPAND | wxALL, 5);

	// - stereoscopy

	if (vconfig.backend_info.bSupportsGeometryShaders)
	{
		wxFlexGridSizer* const szr_stereo = new wxFlexGridSizer(2, 5, 5);

		szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Stereoscopic 3D Mode:")), 1, wxALIGN_CENTER_VERTICAL, 0);

		const wxString stereo_choices[] = { _("Off"), _("Side-by-Side"), _("Top-and-Bottom"), _("Anaglyph"), _("Nvidia 3D Vision") };
		wxChoice* stereo_choice = CreateChoice(page_enh, vconfig.iStereoMode, wxGetTranslation(stereo_3d_desc), vconfig.backend_info.bSupports3DVision ? ArraySize(stereo_choices) : ArraySize(stereo_choices) - 1, stereo_choices);
		stereo_choice->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_StereoMode, this);
		szr_stereo->Add(stereo_choice);

		wxSlider* const sep_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iStereoDepth, 0, 100, wxDefaultPosition, wxDefaultSize);
		sep_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_StereoDepth, this);
		RegisterControl(sep_slider, wxGetTranslation(stereo_depth_desc));

		szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Depth:")), 1, wxALIGN_CENTER_VERTICAL, 0);
		szr_stereo->Add(sep_slider, 0, wxEXPAND | wxRIGHT);

		conv_slider = new wxSlider(page_enh, wxID_ANY, vconfig.iStereoConvergencePercentage, 0, 200, wxDefaultPosition, wxDefaultSize, wxSL_AUTOTICKS);
		conv_slider->ClearTicks();
		conv_slider->SetTick(100);
		conv_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_StereoConvergence, this);
		RegisterControl(conv_slider, wxGetTranslation(stereo_convergence_desc));

		szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Convergence:")), 1, wxALIGN_CENTER_VERTICAL, 0);
		szr_stereo->Add(conv_slider, 0, wxEXPAND | wxRIGHT);

		szr_stereo->Add(CreateCheckBox(page_enh, _("Swap Eyes"), wxGetTranslation(stereo_swap_desc), vconfig.bStereoSwapEyes));

		wxStaticBoxSizer* const group_stereo = new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Stereoscopy"));
		group_stereo->Add(szr_stereo, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
		szr_enh_main->Add(group_stereo, 0, wxEXPAND | wxALL, 5);
	}

	szr_enh_main->AddStretchSpacer();
	CreateDescriptionArea(page_enh, szr_enh_main);
	page_enh->SetSizerAndFit(szr_enh_main);
	}


	// -- SPEED HACKS --
	{
	wxPanel* const page_hacks = new wxPanel(notebook);
	notebook->AddPage(page_hacks, _("Hacks"));
	wxBoxSizer* const szr_hacks = new wxBoxSizer(wxVERTICAL);

	// - EFB hacks
	wxStaticBoxSizer* const szr_efb = new wxStaticBoxSizer(wxVERTICAL, page_hacks, _("Embedded Frame Buffer (EFB)"));

	szr_efb->Add(CreateCheckBox(page_hacks, _("Skip EFB Access from CPU"), wxGetTranslation(efb_access_desc), vconfig.bEFBAccessEnable, true), 0, wxBOTTOM | wxLEFT, 5);
	szr_efb->Add(CreateCheckBox(page_hacks, _("Ignore Format Changes"), wxGetTranslation(efb_emulate_format_changes_desc), vconfig.bEFBEmulateFormatChanges, true), 0, wxBOTTOM | wxLEFT, 5);
	szr_efb->Add(CreateCheckBox(page_hacks, _("Store EFB Copies to Texture Only"), wxGetTranslation(skip_efb_copy_to_ram_desc), vconfig.bSkipEFBCopyToRam), 0, wxBOTTOM | wxLEFT, 5);

	szr_hacks->Add(szr_efb, 0, wxEXPAND | wxALL, 5);

	// Texture cache
	{
	wxStaticBoxSizer* const szr_safetex = new wxStaticBoxSizer(wxHORIZONTAL, page_hacks, _("Texture Cache"));

	// TODO: Use wxSL_MIN_MAX_LABELS or wxSL_VALUE_LABEL with wx 2.9.1
	wxSlider* const stc_slider = new wxSlider(page_hacks, wxID_ANY, 0, 0, 2, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL|wxSL_BOTTOM);
	stc_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_Stc, this);
	RegisterControl(stc_slider, wxGetTranslation(stc_desc));

	if (vconfig.iSafeTextureCache_ColorSamples == 0) stc_slider->SetValue(0);
	else if (vconfig.iSafeTextureCache_ColorSamples == 512) stc_slider->SetValue(1);
	else if (vconfig.iSafeTextureCache_ColorSamples == 128) stc_slider->SetValue(2);
	else stc_slider->Disable(); // Using custom number of samples; TODO: Inform the user why this is disabled..

	szr_safetex->Add(new wxStaticText(page_hacks, wxID_ANY, _("Accuracy:")), 0, wxALL, 5);
	szr_safetex->AddStretchSpacer(1);
	szr_safetex->Add(new wxStaticText(page_hacks, wxID_ANY, _("Safe")), 0, wxLEFT|wxTOP|wxBOTTOM, 5);
	szr_safetex->Add(stc_slider, 2, wxRIGHT, 0);
	szr_safetex->Add(new wxStaticText(page_hacks, wxID_ANY, _("Fast")), 0, wxRIGHT|wxTOP|wxBOTTOM, 5);
	szr_hacks->Add(szr_safetex, 0, wxEXPAND | wxALL, 5);
	}

	// - XFB
	{
	wxStaticBoxSizer* const group_xfb = new wxStaticBoxSizer(wxHORIZONTAL, page_hacks, _("External Frame Buffer (XFB)"));

	SettingCheckBox* disable_xfb = CreateCheckBox(page_hacks, _("Disable"), wxGetTranslation(xfb_desc), vconfig.bUseXFB, true);
	virtual_xfb = CreateRadioButton(page_hacks, _("Virtual"), wxGetTranslation(xfb_virtual_desc), vconfig.bUseRealXFB, true, wxRB_GROUP);
	real_xfb = CreateRadioButton(page_hacks, _("Real"), wxGetTranslation(xfb_real_desc), vconfig.bUseRealXFB);

	group_xfb->Add(disable_xfb, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	group_xfb->AddStretchSpacer(1);
	group_xfb->Add(virtual_xfb, 0, wxRIGHT, 5);
	group_xfb->Add(real_xfb, 0, wxRIGHT, 5);
	szr_hacks->Add(group_xfb, 0, wxEXPAND | wxALL, 5);
	} // xfb

	// - other hacks
	{
	wxGridSizer* const szr_other = new wxGridSizer(2, 5, 5);
	szr_other->Add(CreateCheckBox(page_hacks, _("Fast Depth Calculation"), wxGetTranslation(fast_depth_calc_desc), vconfig.bFastDepthCalc));
	szr_other->Add(CreateCheckBox(page_hacks, _("Disable Bounding Box"), wxGetTranslation(disable_bbox_desc), vconfig.bBBoxEnable, true));

	wxStaticBoxSizer* const group_other = new wxStaticBoxSizer(wxVERTICAL, page_hacks, _("Other"));
	group_other->Add(szr_other, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	szr_hacks->Add(group_other, 0, wxEXPAND | wxALL, 5);
	}

	szr_hacks->AddStretchSpacer();
	CreateDescriptionArea(page_hacks, szr_hacks);
	page_hacks->SetSizerAndFit(szr_hacks);
	}

	// -- ADVANCED --
	{
	wxPanel* const page_advanced = new wxPanel(notebook);
	notebook->AddPage(page_advanced, _("Advanced"));
	wxBoxSizer* const szr_advanced = new wxBoxSizer(wxVERTICAL);

	// - debug
	{
	wxGridSizer* const szr_debug = new wxGridSizer(2, 5, 5);

	szr_debug->Add(CreateCheckBox(page_advanced, _("Enable Wireframe"), wxGetTranslation(wireframe_desc), vconfig.bWireFrame));
	szr_debug->Add(CreateCheckBox(page_advanced, _("Show Statistics"), wxGetTranslation(show_stats_desc), vconfig.bOverlayStats));
	szr_debug->Add(CreateCheckBox(page_advanced, _("Texture Format Overlay"), wxGetTranslation(texfmt_desc), vconfig.bTexFmtOverlayEnable));

	wxStaticBoxSizer* const group_debug = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Debugging"));
	szr_advanced->Add(group_debug, 0, wxEXPAND | wxALL, 5);
	group_debug->Add(szr_debug, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	}

	// - utility
	{
	wxGridSizer* const szr_utility = new wxGridSizer(2, 5, 5);

	szr_utility->Add(CreateCheckBox(page_advanced, _("Dump Textures"), wxGetTranslation(dump_textures_desc), vconfig.bDumpTextures));
	szr_utility->Add(CreateCheckBox(page_advanced, _("Load Custom Textures"), wxGetTranslation(load_hires_textures_desc), vconfig.bHiresTextures));
	cache_hires_textures = CreateCheckBox(page_advanced, _("Prefetch Custom Textures"), wxGetTranslation(cache_hires_textures_desc), vconfig.bCacheHiresTextures);
	szr_utility->Add(cache_hires_textures);
	szr_utility->Add(CreateCheckBox(page_advanced, _("Dump EFB Target"), wxGetTranslation(dump_efb_desc), vconfig.bDumpEFBTarget));
	szr_utility->Add(CreateCheckBox(page_advanced, _("Free Look"), wxGetTranslation(free_look_desc), vconfig.bFreeLook));
#if defined(HAVE_LIBAV) || defined (_WIN32)
	szr_utility->Add(CreateCheckBox(page_advanced, _("Frame Dumps use FFV1"), wxGetTranslation(use_ffv1_desc), vconfig.bUseFFV1));
#endif

	wxStaticBoxSizer* const group_utility = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Utility"));
	szr_advanced->Add(group_utility, 0, wxEXPAND | wxALL, 5);
	group_utility->Add(szr_utility, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	}

	// - misc
	{
	wxGridSizer* const szr_misc = new wxGridSizer(2, 5, 5);

	szr_misc->Add(CreateCheckBox(page_advanced, _("Crop"), wxGetTranslation(crop_desc), vconfig.bCrop));

	// Progressive Scan
	{
	progressive_scan_checkbox = new wxCheckBox(page_advanced, wxID_ANY, _("Enable Progressive Scan"));
	RegisterControl(progressive_scan_checkbox, wxGetTranslation(prog_scan_desc));
	progressive_scan_checkbox->Bind(wxEVT_CHECKBOX, &VideoConfigDiag::Event_ProgressiveScan, this);

	progressive_scan_checkbox->SetValue(SConfig::GetInstance().bProgressive);
	// A bit strange behavior, but this needs to stay in sync with the main progressive boolean; TODO: Is this still necessary?
	SConfig::GetInstance().m_SYSCONF->SetData("IPL.PGS", SConfig::GetInstance().bProgressive);

	szr_misc->Add(progressive_scan_checkbox);
	}

#if defined WIN32
	// Borderless Fullscreen
	borderless_fullscreen = CreateCheckBox(page_advanced, _("Borderless Fullscreen"), wxGetTranslation(borderless_fullscreen_desc), vconfig.bBorderlessFullscreen);
	szr_misc->Add(borderless_fullscreen);
#endif

	wxStaticBoxSizer* const group_misc = new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Misc"));
	szr_advanced->Add(group_misc, 0, wxEXPAND | wxALL, 5);
	group_misc->Add(szr_misc, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	}

	szr_advanced->AddStretchSpacer();
	CreateDescriptionArea(page_advanced, szr_advanced);
	page_advanced->SetSizerAndFit(szr_advanced);
	}

	wxButton* const btn_close = new wxButton(this, wxID_OK, _("Close"));
	btn_close->Bind(wxEVT_BUTTON, &VideoConfigDiag::Event_ClickClose, this);

	Bind(wxEVT_CLOSE_WINDOW, &VideoConfigDiag::Event_Close, this);

	wxBoxSizer* const szr_main = new wxBoxSizer(wxVERTICAL);
	szr_main->Add(notebook, 1, wxEXPAND | wxALL, 5);
	szr_main->Add(btn_close, 0, wxALIGN_RIGHT | wxRIGHT | wxBOTTOM, 5);

	SetSizerAndFit(szr_main);
	Center();
	SetFocus();

	UpdateWindowUI();
}

void VideoConfigDiag::Event_DisplayResolution(wxCommandEvent &ev)
{
	SConfig::GetInstance().strFullscreenResolution =
		WxStrToStr(choice_display_resolution->GetStringSelection());
#if defined(HAVE_XRANDR) && HAVE_XRANDR
	main_frame->m_XRRConfig->Update();
#endif
	ev.Skip();
}

SettingCheckBox* VideoConfigDiag::CreateCheckBox(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse, long style)
{
	SettingCheckBox* const cb = new SettingCheckBox(parent, label, wxString(), setting, reverse, style);
	RegisterControl(cb, description);
	return cb;
}

SettingChoice* VideoConfigDiag::CreateChoice(wxWindow* parent, int& setting, const wxString& description, int num, const wxString choices[], long style)
{
	SettingChoice* const ch = new SettingChoice(parent, setting, wxString(), num, choices, style);
	RegisterControl(ch, description);
	return ch;
}

SettingRadioButton* VideoConfigDiag::CreateRadioButton(wxWindow* parent, const wxString& label, const wxString& description, bool &setting, bool reverse, long style)
{
	SettingRadioButton* const rb = new SettingRadioButton(parent, label, wxString(), setting, reverse, style);
	RegisterControl(rb, description);
	return rb;
}

/* Use this to register descriptions for controls which have NOT been created using the Create* functions from above */
wxControl* VideoConfigDiag::RegisterControl(wxControl* const control, const wxString& description)
{
	ctrl_descs.emplace(control, description);
	control->Bind(wxEVT_ENTER_WINDOW, &VideoConfigDiag::Evt_EnterControl, this);
	control->Bind(wxEVT_LEAVE_WINDOW, &VideoConfigDiag::Evt_LeaveControl, this);
	return control;
}

void VideoConfigDiag::Evt_EnterControl(wxMouseEvent& ev)
{
	// TODO: Re-Fit the sizer if necessary!

	// Get settings control object from event
	wxWindow* ctrl = (wxWindow*)ev.GetEventObject();
	if (!ctrl) return;

	// look up description text object from the control's parent (which is the wxPanel of the current tab)
	wxStaticText* descr_text = desc_texts[ctrl->GetParent()];
	if (!descr_text) return;

	// look up the description of the selected control and assign it to the current description text object's label
	descr_text->SetLabel(ctrl_descs[ctrl]);
	descr_text->Wrap(descr_text->GetContainingSizer()->GetSize().x - 20);

	ev.Skip();
}

// TODO: Don't hardcode the size of the description area via line breaks
#define DEFAULT_DESC_TEXT _("Move the mouse pointer over an option to display a detailed description.\n\n\n\n\n\n\n")
void VideoConfigDiag::Evt_LeaveControl(wxMouseEvent& ev)
{
	// look up description text control and reset its label
	wxWindow* ctrl = (wxWindow*)ev.GetEventObject();
	if (!ctrl) return;
	wxStaticText* descr_text = desc_texts[ctrl->GetParent()];
	if (!descr_text) return;

	descr_text->SetLabel(DEFAULT_DESC_TEXT);
	descr_text->Wrap(descr_text->GetContainingSizer()->GetSize().x - 20);
	ev.Skip();
}

void VideoConfigDiag::CreateDescriptionArea(wxPanel* const page, wxBoxSizer* const sizer)
{
	// Create description frame
	wxStaticBoxSizer* const desc_sizer = new wxStaticBoxSizer(wxVERTICAL, page, _("Description"));
	sizer->Add(desc_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Need to call SetSizerAndFit here, since we don't want the description texts to change the dialog width
	page->SetSizerAndFit(sizer);

	// Create description text
	wxStaticText* const desc_text = new wxStaticText(page, wxID_ANY, DEFAULT_DESC_TEXT);
	desc_text->Wrap(desc_sizer->GetSize().x - 20);
	desc_sizer->Add(desc_text, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Store description text object for later lookup
	desc_texts.emplace(page, desc_text);
}

void VideoConfigDiag::PopulatePostProcessingShaders()
{
	std::vector<std::string> &shaders = (vconfig.iStereoMode == STEREO_ANAGLYPH) ?
			vconfig.backend_info.AnaglyphShaders : vconfig.backend_info.PPShaders;

	if (shaders.empty())
		return;

	choice_ppshader->AppendString(_("(off)"));

	for (const std::string& shader : shaders)
	{
		choice_ppshader->AppendString(StrToWxStr(shader));
	}

	if (!choice_ppshader->SetStringSelection(StrToWxStr(vconfig.sPostProcessingShader)))
	{
		// Invalid shader, reset it to default
		choice_ppshader->Select(0);

		if (vconfig.iStereoMode == STEREO_ANAGLYPH)
		{
			vconfig.sPostProcessingShader = "dubois";
			choice_ppshader->SetStringSelection(StrToWxStr(vconfig.sPostProcessingShader));
		}
		else
			vconfig.sPostProcessingShader.clear();
	}

	// Should the configuration button be loaded by default?
	PostProcessingShaderConfiguration postprocessing_shader;
	postprocessing_shader.LoadShader(vconfig.sPostProcessingShader);
	button_config_pp->Enable(postprocessing_shader.HasOptions());
}

void VideoConfigDiag::PopulateAAList()
{
	const std::vector<int>& aa_modes = vconfig.backend_info.AAModes;
	const bool supports_ssaa = vconfig.backend_info.bSupportsSSAA;
	m_msaa_modes = 0;

	for (int mode : aa_modes)
	{
		if (mode == 1)
		{
			choice_aamode->AppendString(_("None"));
			_assert_msg_(VIDEO, !supports_ssaa || m_msaa_modes == 0, "SSAA setting won't work correctly");
		}
		else
		{
			choice_aamode->AppendString(std::to_string(mode) + "x MSAA");
			++m_msaa_modes;
		}
	}

	if (supports_ssaa)
	{
		for (int mode : aa_modes)
		{
			if (mode != 1)
				choice_aamode->AppendString(std::to_string(mode) + "x SSAA");
		}
	}

	int selected_mode_index = 0;

	auto index = std::find(aa_modes.begin(), aa_modes.end(), vconfig.iMultisamples);
	if (index != aa_modes.end())
		selected_mode_index = index - aa_modes.begin();

	// Select one of the SSAA modes at the end of the list if SSAA is enabled
	if (supports_ssaa && vconfig.bSSAA && aa_modes[selected_mode_index] != 1)
		selected_mode_index += m_msaa_modes;

	choice_aamode->SetSelection(selected_mode_index);
}

void VideoConfigDiag::OnAAChanged(wxCommandEvent& ev)
{
	size_t mode = ev.GetInt();
	ev.Skip();

	vconfig.bSSAA = mode > m_msaa_modes;
	mode -= vconfig.bSSAA * m_msaa_modes;

	if (mode >= vconfig.backend_info.AAModes.size())
		return;

	vconfig.iMultisamples = vconfig.backend_info.AAModes[mode];
}
