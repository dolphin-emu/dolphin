// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/VideoConfigDiag.h"

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/control.h>
#include <wx/dialog.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/SysConf.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinWX/DolphinSlider.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/PostProcessingConfigDiag.h"
#include "DolphinWX/WxUtils.h"
#include "UICommon/VideoUtils.h"
#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VR.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#endif

// template instantiation
template class BoolSetting<wxCheckBox>;
template class BoolSetting<wxRadioButton>;

template <>
SettingCheckBox::BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip,
                             const Config::ConfigInfo<bool>& setting, bool reverse, long style)
    : wxCheckBox(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, style),
      m_setting(setting), m_reverse(reverse)
{
  SetToolTip(tooltip);
  SetValue(Config::Get(m_setting) ^ m_reverse);
  if (Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base)
    SetFont(GetFont().MakeBold());
  Bind(wxEVT_CHECKBOX, &SettingCheckBox::UpdateValue, this);
}

template <>
SettingRadioButton::BoolSetting(wxWindow* parent, const wxString& label, const wxString& tooltip,
                                const Config::ConfigInfo<bool>& setting, bool reverse, long style)
    : wxRadioButton(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, style),
      m_setting(setting), m_reverse(reverse)
{
  SetToolTip(tooltip);
  SetValue(Config::Get(m_setting) ^ m_reverse);
  if (Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base)
    SetFont(GetFont().MakeBold());
  Bind(wxEVT_RADIOBUTTON, &SettingRadioButton::UpdateValue, this);
}

template <typename T>
FloatSetting<T>::FloatSetting(wxWindow* parent, const wxString& label,
                              const Config::ConfigInfo<T>& setting, T minVal, T maxVal, T increment,
                              long style)
    : wxSpinCtrlDouble(parent, -1, label, wxDefaultPosition, wxDefaultSize, style, minVal, maxVal,
                       Config::Get(setting), increment),
      m_setting(setting)
{
  SetValue(Config::Get(m_setting));
  Bind(wxEVT_SPINCTRLDOUBLE, &FloatSetting::UpdateValue, this);
}

template <>
RefBoolSetting<wxCheckBox>::RefBoolSetting(wxWindow* parent, const wxString& label,
                                           const wxString& tooltip, bool& setting, bool reverse,
                                           long style)
    : wxCheckBox(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, style),
      m_setting(setting), m_reverse(reverse)
{
  SetToolTip(tooltip);
  SetValue(m_setting ^ m_reverse);
  Bind(wxEVT_CHECKBOX, &RefBoolSetting<wxCheckBox>::UpdateValue, this);
}

SettingChoice::SettingChoice(wxWindow* parent, const Config::ConfigInfo<int>& setting,
                             const wxString& tooltip, int num, const wxString choices[], long style)
    : wxChoice(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, num, choices), m_setting(setting)
{
  SetToolTip(tooltip);
  Select(Config::Get(m_setting));
  Bind(wxEVT_CHOICE, &SettingChoice::UpdateValue, this);
}

void SettingChoice::UpdateValue(wxCommandEvent& ev)
{
  Config::SetBaseOrCurrent(m_setting, ev.GetInt());
  ev.Skip();
}

void VideoConfigDiag::Event_ClickSave(wxCommandEvent&)
{
  if (SConfig::GetInstance().GetGameID() != "")
    g_Config.GameIniSave();
}

static wxString default_desc =
    wxTRANSLATE("Move the mouse pointer over an option to display a detailed description.");
#if defined(_WIN32)
static wxString backend_desc =
    wxTRANSLATE("Selects what graphics API to use internally.\nThe software renderer is extremely "
                "slow and only useful for debugging, so you'll want to use either Direct3D or "
                "OpenGL. Different games and different GPUs will behave differently on each "
                "backend, so for the best emulation experience it's recommended to try both and "
                "choose the one that's less problematic.\n\nIf unsure, select OpenGL.");
#else
static wxString backend_desc =
    wxTRANSLATE("Selects what graphics API to use internally.\nThe software renderer is extremely "
                "slow and only useful for debugging, so unless you have a reason to use it you'll "
                "want to select OpenGL here.\n\nIf unsure, select OpenGL.");
#endif
static wxString adapter_desc =
    wxTRANSLATE("Selects a hardware adapter to use.\n\nIf unsure, use the first one.");
static wxString display_res_desc =
    wxTRANSLATE("Selects the display resolution used in fullscreen mode.\nThis should always be "
                "bigger than or equal to the internal resolution. Performance impact is "
                "negligible.\n\nIf unsure, select auto.");
static wxString use_fullscreen_desc = wxTRANSLATE(
    "Enable this if you want the whole screen to be used for rendering.\nIf this is disabled, a "
    "render window will be created instead.\n\nIf unsure, leave this unchecked.");
static wxString auto_window_size_desc =
    wxTRANSLATE("Automatically adjusts the window size to your internal resolution.\n\nIf unsure, "
                "leave this unchecked.");
static wxString keep_window_on_top_desc = wxTRANSLATE(
    "Keep the game window on top of all other windows.\n\nIf unsure, leave this unchecked.");
static wxString hide_mouse_cursor_desc =
    wxTRANSLATE("Hides the mouse cursor if it's on top of the emulation window.\n\nIf unsure, "
                "leave this unchecked.");
static wxString render_to_main_win_desc =
    wxTRANSLATE("Enable this if you want to use the main Dolphin window for rendering rather than "
                "a separate render window.\n\nIf unsure, leave this unchecked.");
static wxString prog_scan_desc =
    wxTRANSLATE("Enables progressive scan if supported by the emulated software.\nMost games don't "
                "care about this.\n\nIf unsure, leave this unchecked.");
static wxString ar_desc =
    wxTRANSLATE("Select what aspect ratio to use when rendering:\nAuto: Use the native aspect "
                "ratio\nForce 16:9: Mimic an analog TV with a widescreen aspect ratio.\nForce 4:3: "
                "Mimic a standard 4:3 analog TV.\nStretch to Window: Stretch the picture to the "
                "window size.\n\nIf unsure, select Auto.");
static wxString ws_hack_desc = wxTRANSLATE(
    "Forces the game to output graphics for any aspect ratio.\nUse with \"Aspect Ratio\" set to "
    "\"Force 16:9\" to force 4:3-only games to run at 16:9.\nRarely produces good results and "
    "often partially breaks graphics and game UIs.\nUnnecessary (and detrimental) if using any "
    "AR/Gecko-code widescreen patches.\n\nIf unsure, leave this unchecked.");
static wxString vsync_desc =
    wxTRANSLATE("Wait for vertical blanks in order to reduce tearing.\nDecreases performance if "
                "emulation speed is below 100%.\n\nIf unsure, leave this unchecked.");
static wxString af_desc = wxTRANSLATE(
    "Enable anisotropic filtering.\nEnhances visual quality of textures that are at oblique "
    "viewing angles.\nMight cause issues in a small number of games.\n\nIf unsure, select 1x.");
static wxString aa_desc =
    wxTRANSLATE("Reduces the amount of aliasing caused by rasterizing 3D graphics. This smooths "
                "out jagged edges on objects.\nIncreases GPU load and sometimes causes graphical "
                "issues. SSAA is significantly more demanding than MSAA, but provides top quality "
                "geometry anti-aliasing and also applies anti-aliasing to lighting, shader "
                "effects, and textures.\n\nIf unsure, select None.");
static wxString scaled_efb_copy_desc = wxTRANSLATE(
    "Greatly increases quality of textures generated using render-to-texture effects.\nRaising the "
    "internal resolution will improve the effect of this setting.\nSlightly increases GPU load and "
    "causes relatively few graphical issues.\n\nIf unsure, leave this checked.");
static wxString pixel_lighting_desc = wxTRANSLATE(
    "Calculates lighting of 3D objects per-pixel rather than per-vertex, smoothing out the "
    "appearance of lit polygons and making individual triangles less noticeable.\nRarely causes "
    "slowdowns or graphical issues.\n\nIf unsure, leave this unchecked.");
static wxString fast_depth_calc_desc =
    wxTRANSLATE("Use a less accurate algorithm to calculate depth values.\nCauses issues in a few "
                "games, but can give a decent speedup depending on the game and/or your GPU.\n\nIf "
                "unsure, leave this checked.");
static wxString disable_bbox_desc =
    wxTRANSLATE("Disable the bounding box emulation.\nThis may improve the GPU performance a lot, "
                "but some games will break.\n\nIf unsure, leave this checked.");
static wxString force_filtering_desc =
    wxTRANSLATE("Filter all textures, including any that the game explicitly set as "
                "unfiltered.\nMay improve quality of certain textures in some games, but will "
                "cause issues in others.\n\nIf unsure, leave this unchecked.");
static wxString borderless_fullscreen_desc = wxTRANSLATE(
    "Implement fullscreen mode with a borderless window spanning the whole screen instead of using "
    "exclusive mode.\nAllows for faster transitions between fullscreen and windowed mode, but "
    "slightly increases input latency, makes movement less smooth and slightly decreases "
    "performance.\nExclusive mode is required for Nvidia 3D Vision to work in the Direct3D "
    "backend.\n\nIf unsure, leave this unchecked.");
static wxString internal_res_desc =
    wxTRANSLATE("Specifies the resolution used to render at. A high resolution greatly improves "
                "visual quality, but also greatly increases GPU load and can cause issues in "
                "certain games.\n\"Multiple of 640x528\" will result in a size slightly larger "
                "than \"Window Size\" but yield fewer issues. Generally speaking, the lower the "
                "internal resolution is, the better your performance will be. Auto (Window Size), "
                "1.5x, and 2.5x may cause issues in some games.\n\nIf unsure, select Native.");
static wxString efb_access_desc =
    wxTRANSLATE("Ignore any requests from the CPU to read from or write to the EFB.\nImproves "
                "performance in some games, but might disable some gameplay-related features or "
                "graphical effects.\n\nIf unsure, leave this unchecked.");
static wxString efb_copy_desc = wxTRANSLATE(
    "Disable emulation of EFB copies.\nThese are often used for post-processing or "
    "render-to-texture effects, so while checking this setting may give a minor speedup over EFB "
    "to Texture it almost invariably also causes issues.\n\nIf unsure, leave this unchecked.");
static wxString efb_emulate_format_changes_desc =
    wxTRANSLATE("Ignore any changes to the EFB format.\nImproves performance in many games without "
                "any negative effect. Causes graphical defects in a small number of other "
                "games.\n\nIf unsure, leave this checked.");
static wxString efb_copy_clear_desc =
    wxTRANSLATE("Disables the black box or screen that appears where an EFB copy should have been "
                "rendered.  Use only if you are seeing a black box or screen after disabling EFB "
                "copies, or if a game is not rendering properly.  May cause artifacts such as bad "
                "blending on shadows or phantom images.\n\nIf unsure, leave this unchecked.");
static wxString efb_copy_texture_desc =
    wxTRANSLATE("Store EFB copies in GPU texture objects.\nThis isn't particularly accurate, but "
                "it works well enough for most games and gives a great speedup over EFB to "
                "RAM.\n\nIf unsure, leave this checked.");
static wxString efb_copy_ram_desc =
    wxTRANSLATE("Accurately emulate EFB copies.\nNumerous games depend on this for certain "
                "graphical effects or gameplay functionality.\nThis is much slower than EFB to "
                "Texture.\n\nIf unsure, check EFB to Texture instead.");
static wxString skip_efb_copy_to_ram_desc = wxTRANSLATE(
    "Stores EFB Copies exclusively on the GPU, bypassing system memory. Causes graphical defects "
    "in a small number of games.\n\nEnabled = EFB Copies to Texture\nDisabled = EFB Copies to RAM "
    "(and Texture)\n\nIf unsure, leave this checked.");
static wxString stc_desc =
    wxTRANSLATE("The \"Safe\" setting eliminates the likelihood of the GPU missing texture updates "
                "from RAM.\nLower accuracies cause in-game text to appear garbled in certain "
                "games.\n\nIf unsure, use the rightmost value.");
static wxString wireframe_desc =
    wxTRANSLATE("Render the scene as a wireframe.\n\nIf unsure, leave this unchecked.");
static wxString disable_fog_desc =
    wxTRANSLATE("Makes distant objects more visible by removing fog, thus increasing the overall "
                "detail.\nDisabling fog will break some games which rely on proper fog "
                "emulation.\n\nIf unsure, leave this unchecked.");
static wxString show_fps_desc =
    wxTRANSLATE("Show the number of frames rendered per second as a measure of "
                "emulation speed.\n\nIf unsure, leave this unchecked.");
static wxString show_netplay_ping_desc =
    wxTRANSLATE("Show the players' maximum Ping while playing on "
                "NetPlay.\n\nIf unsure, leave this unchecked.");
static wxString log_render_time_to_file_desc =
    wxTRANSLATE("Log the render time of every frame to User/Logs/render_time.txt. Use this "
                "feature when you want to measure the performance of Dolphin.\n\nIf "
                "unsure, leave this unchecked.");
static wxString show_stats_desc =
    wxTRANSLATE("Show various rendering statistics.\n\nIf unsure, leave this unchecked.");
static wxString show_netplay_messages_desc =
    wxTRANSLATE("When playing on NetPlay, show chat messages, buffer changes and "
                "desync alerts.\n\nIf unsure, leave this unchecked.");
static wxString texfmt_desc =
    wxTRANSLATE("Modify textures to show the format they're encoded in. Needs an emulation reset "
                "in most cases.\n\nIf unsure, leave this unchecked.");
static wxString xfb_desc = wxTRANSLATE(
    "Disable any XFB emulation.\nSpeeds up emulation a lot but causes heavy glitches in many games "
    "which rely on them (especially homebrew applications).\n\nIf unsure, leave this checked.");
static wxString xfb_virtual_desc = wxTRANSLATE(
    "Emulate XFBs using GPU texture objects.\nFixes many games which don't work without XFB "
    "emulation while not being as slow as real XFB emulation. However, it may still fail for a lot "
    "of other games (especially homebrew applications).\n\nIf unsure, leave this checked.");
static wxString xfb_real_desc =
    wxTRANSLATE("Emulate XFBs accurately.\nSlows down emulation a lot and prohibits "
                "high-resolution rendering but is necessary to emulate a number of games "
                "properly.\n\nIf unsure, check virtual XFB emulation instead.");
static wxString dump_textures_desc =
    wxTRANSLATE("Dump decoded game textures to User/Dump/Textures/<game_id>/.\n\nIf unsure, leave "
                "this unchecked.");
static wxString load_hires_textures_desc = wxTRANSLATE(
    "Load custom textures from User/Load/Textures/<game_id>/.\n\nIf unsure, leave this unchecked.");
static wxString cache_hires_textures_desc =
    wxTRANSLATE("Cache custom textures to system RAM on startup.\nThis can require exponentially "
                "more RAM but fixes possible stuttering.\n\nIf unsure, leave this unchecked.");
static wxString dump_efb_desc = wxTRANSLATE(
    "Dump the contents of EFB copies to User/Dump/Textures/.\n\nIf unsure, leave this unchecked.");
static wxString internal_resolution_frame_dumping_desc = wxTRANSLATE(
    "Create frame dumps and screenshots at the internal resolution of the renderer, rather than "
    "the size of the window it is displayed within. If the aspect ratio is widescreen, the output "
    "image will be scaled horizontally to preserve the vertical resolution.\n\nIf unsure, leave "
    "this unchecked.");
#if defined(HAVE_FFMPEG)
static wxString use_ffv1_desc =
    wxTRANSLATE("Encode frame dumps using the FFV1 codec.\n\nIf unsure, leave this unchecked.");
#endif
static wxString free_look_desc =
    wxTRANSLATE("This feature allows you to change the game's camera with the mouse.\nMove the "
                "mouse while holding the right mouse button to pan and while holding the middle "
                "button to move.\n\nIf unsure, leave this unchecked.");
static wxString crop_desc = wxTRANSLATE("Crop the picture from its native aspect ratio to 4:3 or "
                                        "16:9.\n\nIf unsure, leave this unchecked.");
static wxString ppshader_desc = wxTRANSLATE(
    "Apply a post-processing effect after finishing a frame.\n\nIf unsure, select Off.");
static wxString cache_efb_copies_desc =
    wxTRANSLATE("Slightly speeds up EFB to RAM copies by sacrificing emulation accuracy.\nIf "
                "you're experiencing any issues, try raising texture cache accuracy or disable "
                "this option.\n\nIf unsure, leave this unchecked.");
static wxString stereo_3d_desc =
    wxTRANSLATE("Selects the stereoscopic 3D mode. Stereoscopy allows you to get a better feeling "
                "of depth if you have the necessary hardware.\nSide-by-Side and Top-and-Bottom are "
                "used by most 3D TVs.\nAnaglyph is used for Red-Cyan colored glasses.\nHDMI 3D is "
                "used when your monitor supports 3D display resolutions.\nHeavily decreases "
                "emulation speed and sometimes causes issues.\n\nIf unsure, select Off.");
static wxString stereo_depth_desc =
    wxTRANSLATE("Controls the separation distance between the virtual cameras.\nA higher value "
                "creates a stronger feeling of depth while a lower value is more comfortable.");
static wxString stereo_convergence_desc =
    wxTRANSLATE("Controls the distance of the convergence plane. This is the distance at which "
                "virtual objects will appear to be in front of the screen.\nA higher value creates "
                "stronger out-of-screen effects while a lower value is more comfortable.");
static wxString stereo_swap_desc =
    wxTRANSLATE("Swaps the left and right eye. Mostly useful if you want to view side-by-side "
                "cross-eyed.\n\nIf unsure, leave this unchecked.");
static wxString validation_layer_desc =
    wxTRANSLATE("Enables validation of API calls made by the video backend, which may assist in "
                "debugging graphical issues.\n\nIf unsure, leave this unchecked.");
static wxString backend_multithreading_desc =
    wxTRANSLATE("Enables multi-threading in the video backend, which may result in performance "
                "gains in some scenarios.\n\nIf unsure, leave this unchecked.");
static wxString true_color_desc =
    wxTRANSLATE("Forces the game to render the RGB color channels in 24-bit, thereby increasing "
                "quality by reducing color banding.\nIt has no impact on performance and causes "
                "few graphical issues.\n\n\nIf unsure, leave this checked.");
static wxString vertex_rounding_desc =
    wxTRANSLATE("Rounds 2D vertices to whole pixels. Fixes graphical problems in some games at "
                "higher internal resolutions. This setting has no effect when native internal "
                "resolution is used.\n\nIf unsure, leave this unchecked.");
static wxString gpu_texture_decoding_desc =
    wxTRANSLATE("Enables texture decoding using the GPU instead of the CPU. This may result in "
                "performance gains in some scenarios, or on systems where the CPU is the "
                "bottleneck.\n\nIf unsure, leave this unchecked.");
static wxString ubershader_desc =
    wxTRANSLATE("Disabled: Ubershaders are never used. Stuttering will occur during shader "
                "compilation, but GPU demands are low. Recommended for low-end hardware.\n\n"
                "Hybrid: Ubershaders will be used to prevent stuttering during shader "
                "compilation, but traditional shaders will be used when they will not cause "
                "stuttering. Balances performance and smoothness.\n\n"
                "Exclusive: Ubershaders will always be used. Only recommended for high-end "
                "systems.");

#if 0
#if !defined(__APPLE__)
// Search for available resolutions - TODO: Move to Common?
// No, now it depends on VR to know which display to check, so don't move it to Common.
// g_hmd_device_name will be nullptr unless there is a VR display attached.
static wxArrayString GetListOfResolutions()
{
  wxArrayString retlist;
  retlist.Add(_("Auto"));
#ifdef _WIN32
  DWORD iModeNum = 0;
  DEVMODEA dmi;
  ZeroMemory(&dmi, sizeof(dmi));
  dmi.dmSize = sizeof(dmi);
  std::vector<std::string> resos;

  while (EnumDisplaySettingsA(g_hmd_device_name, iModeNum++, &dmi) != 0)
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
  }
#elif defined(HAVE_XRANDR) && HAVE_XRANDR
  std::vector<std::string> resos;
  main_frame->m_xrr_config->AddResolutions(resos);
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
#endif

VideoConfigDiag::VideoConfigDiag(wxWindow* parent, const std::string& title)
    : wxDialog(parent, wxID_ANY,
               wxString::Format(_("Dolphin %s Graphics Configuration"),
                                wxGetTranslation(StrToWxStr(title)))),
      vconfig(g_Config)
{
  // We don't need to load the config if the core is running, since it would have been done
  // at startup time already.
  if (!Core::IsRunning())
    vconfig.Refresh();

  Bind(wxEVT_UPDATE_UI, &VideoConfigDiag::OnUpdateUI, this);

  wxNotebook* const notebook = new wxNotebook(this, wxID_ANY);
  const int space5 = FromDIP(5);

  // -- GENERAL --
  {
    wxPanel* const page_general = new wxPanel(notebook);
    notebook->AddPage(page_general, _("General"));
    wxBoxSizer* const szr_general = new wxBoxSizer(wxVERTICAL);

    // - basic
    {
      wxFlexGridSizer* const szr_basic = new wxFlexGridSizer(2, space5, space5);

      // backend
      {
        label_backend = new wxStaticText(page_general, wxID_ANY, _("Backend:"));
        choice_backend = new wxChoice(page_general, wxID_ANY);
        RegisterControl(choice_backend, wxGetTranslation(backend_desc));

        for (const auto& backend : g_available_video_backends)
        {
          choice_backend->AppendString(wxGetTranslation(StrToWxStr(backend->GetDisplayName())));
        }

        choice_backend->SetStringSelection(
            wxGetTranslation(StrToWxStr(g_video_backend->GetDisplayName())));
        choice_backend->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_Backend, this);

        szr_basic->Add(label_backend, 0, wxALIGN_CENTER_VERTICAL);
        szr_basic->Add(choice_backend, 0, wxALIGN_CENTER_VERTICAL);
      }

      // adapter (D3D only)
      if (vconfig.backend_info.Adapters.size())
      {
        choice_adapter =
            CreateChoice(page_general, Config::GFX_ADAPTER, wxGetTranslation(adapter_desc));

        for (const std::string& adapter : vconfig.backend_info.Adapters)
        {
          choice_adapter->AppendString(StrToWxStr(adapter));
        }

        choice_adapter->Select(vconfig.iAdapter);

        label_adapter = new wxStaticText(page_general, wxID_ANY, _("Adapter:"));
        szr_basic->Add(label_adapter, 0, wxALIGN_CENTER_VERTICAL);
        szr_basic->Add(choice_adapter, 0, wxALIGN_CENTER_VERTICAL);
      }

      // - display
      wxFlexGridSizer* const szr_display = new wxFlexGridSizer(2, space5, space5);

      {
#if !defined(__APPLE__)
        // display resolution
        {
          wxArrayString res_list;
          res_list.Add(_("Auto"));
#if defined(HAVE_XRANDR) && HAVE_XRANDR
          const auto resolutions = VideoUtils::GetAvailableResolutions(main_frame->m_xrr_config);
#else
          const auto resolutions = VideoUtils::GetAvailableResolutions(nullptr);
#endif

          for (const auto& res : resolutions)
            res_list.Add(res);

          if (res_list.empty())
            res_list.Add(_("<No resolutions found>"));
          label_display_resolution =
              new wxStaticText(page_general, wxID_ANY, _("Fullscreen Resolution:"));
          choice_display_resolution =
              new wxChoice(page_general, wxID_ANY, wxDefaultPosition, wxDefaultSize, res_list);
          RegisterControl(choice_display_resolution, wxGetTranslation(display_res_desc));
          choice_display_resolution->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_DisplayResolution,
                                          this);

          choice_display_resolution->SetStringSelection(
              StrToWxStr(SConfig::GetInstance().strFullscreenResolution));
          // "Auto" is used as a keyword, convert to translated string
          if (SConfig::GetInstance().strFullscreenResolution == "Auto")
            choice_display_resolution->SetSelection(0);

          szr_display->Add(label_display_resolution, 0, wxALIGN_CENTER_VERTICAL);
          szr_display->Add(choice_display_resolution, 0, wxALIGN_CENTER_VERTICAL);
        }
#endif

        // aspect-ratio
        {
          const wxString ar_choices[] = {_("Auto"), _("Force 16:9"), _("Force 4:3"),
                                         _("Stretch to Window")};

          szr_display->Add(new wxStaticText(page_general, wxID_ANY, _("Aspect Ratio:")), 0,
                           wxALIGN_CENTER_VERTICAL);
          wxChoice* const choice_aspect =
              CreateChoice(page_general, Config::GFX_ASPECT_RATIO, wxGetTranslation(ar_desc),
                           sizeof(ar_choices) / sizeof(*ar_choices), ar_choices);
          szr_display->Add(choice_aspect, 0, wxALIGN_CENTER_VERTICAL);
        }

        // various other display options
        {
          szr_display->Add(CreateCheckBox(page_general, _("V-Sync"), wxGetTranslation(vsync_desc),
                                          Config::GFX_VSYNC));
          szr_display->Add(CreateCheckBoxRefBool(page_general, _("Use Fullscreen"),
                                                 wxGetTranslation(use_fullscreen_desc),
                                                 SConfig::GetInstance().bFullscreen));
        }
      }

      // - other
      wxFlexGridSizer* const szr_other = new wxFlexGridSizer(2, space5, space5);

      {
        szr_other->Add(CreateCheckBox(page_general, _("Show FPS"), wxGetTranslation(show_fps_desc),
                                      Config::GFX_SHOW_FPS));
        szr_other->Add(CreateCheckBox(page_general, _("Show NetPlay Ping"),
                                      wxGetTranslation(show_netplay_ping_desc),
                                      Config::GFX_SHOW_NETPLAY_PING));
        szr_other->Add(CreateCheckBox(page_general, _("Log Render Time to File"),
                                      wxGetTranslation(log_render_time_to_file_desc),
                                      Config::GFX_LOG_RENDER_TIME_TO_FILE));
        szr_other->Add(CreateCheckBoxRefBool(page_general, _("Auto-Adjust Window Size"),
                                             wxGetTranslation(auto_window_size_desc),
                                             SConfig::GetInstance().bRenderWindowAutoSize));
        szr_other->Add(CreateCheckBox(page_general, _("Show NetPlay Messages"),
                                      wxGetTranslation(show_netplay_messages_desc),
                                      Config::GFX_SHOW_NETPLAY_MESSAGES));
        szr_other->Add(CreateCheckBoxRefBool(page_general, _("Keep Window on Top"),
                                             wxGetTranslation(keep_window_on_top_desc),
                                             SConfig::GetInstance().bKeepWindowOnTop));
        szr_other->Add(CreateCheckBoxRefBool(page_general, _("Always Hide Mouse Cursor"),
                                             wxGetTranslation(hide_mouse_cursor_desc),
                                             SConfig::GetInstance().bHideCursor));
        szr_other->Add(render_to_main_checkbox =
                           CreateCheckBoxRefBool(page_general, _("Render to Main Window"),
                                                 wxGetTranslation(render_to_main_win_desc),
                                                 SConfig::GetInstance().bRenderToMain));

        if (vconfig.backend_info.bSupportsMultithreading)
        {
          szr_other->Add(CreateCheckBox(page_general, _("Enable Multi-threading"),
                                        wxGetTranslation(backend_multithreading_desc),
                                        Config::GFX_BACKEND_MULTITHREADING));
        }
      }

      wxStaticBoxSizer* const group_basic =
          new wxStaticBoxSizer(wxVERTICAL, page_general, _("Basic"));
      group_basic->Add(szr_basic, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_basic->AddSpacer(space5);

      wxStaticBoxSizer* const group_display =
          new wxStaticBoxSizer(wxVERTICAL, page_general, _("Display"));
      group_display->Add(szr_display, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_display->AddSpacer(space5);

      wxStaticBoxSizer* const group_other =
          new wxStaticBoxSizer(wxVERTICAL, page_general, _("Other"));
      group_other->Add(szr_other, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_other->AddSpacer(space5);

      szr_general->AddSpacer(space5);
      szr_general->Add(group_basic, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
      szr_general->AddSpacer(space5);
      szr_general->Add(group_display, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
      szr_general->AddSpacer(space5);
      szr_general->Add(group_other, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    }

    szr_general->AddSpacer(space5);
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
    wxGridBagSizer* const szr_enh = new wxGridBagSizer(space5, space5);
    const wxGBSpan span2(1, 2);
    int row = 0;

    // Internal resolution
    {
      const wxString efbscale_choices[] = {_("Auto (Window Size)"),
                                           _("Auto (Multiple of 640x528)"),
                                           _("Native (640x528)"),
                                           _("1.5x Native (960x792)"),
                                           _("2x Native (1280x1056) for 720p"),
                                           _("2.5x Native (1600x1320)"),
                                           _("3x Native (1920x1584) for 1080p"),
                                           _("4x Native (2560x2112) for 1440p"),
                                           _("5x Native (3200x2640)"),
                                           _("6x Native (3840x3168) for 4K"),
                                           _("7x Native (4480x3696)"),
                                           _("8x Native (5120x4224) for 5K"),
                                           _("Custom")};

      wxChoice* const choice_efbscale = CreateChoice(
          page_enh, Config::GFX_EFB_SCALE, wxGetTranslation(internal_res_desc),
          (vconfig.iEFBScale > 11) ? ArraySize(efbscale_choices) : ArraySize(efbscale_choices) - 1,
          efbscale_choices);

      if (vconfig.iEFBScale > 11)
        choice_efbscale->SetSelection(12);

      szr_enh->Add(new wxStaticText(page_enh, wxID_ANY, _("Internal Resolution:")),
                   wxGBPosition(row, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
      szr_enh->Add(choice_efbscale, wxGBPosition(row, 1), span2, wxALIGN_CENTER_VERTICAL);
      row += 1;
    }

    // AA
    {
      text_aamode = new wxStaticText(page_enh, wxID_ANY, _("Anti-Aliasing:"));
      choice_aamode = new wxChoice(page_enh, wxID_ANY);
      RegisterControl(choice_aamode, wxGetTranslation(aa_desc));
      PopulateAAList();
      choice_aamode->Bind(wxEVT_CHOICE, &VideoConfigDiag::OnAAChanged, this);

      szr_enh->Add(text_aamode, wxGBPosition(row, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
      szr_enh->Add(choice_aamode, wxGBPosition(row, 1), span2, wxALIGN_CENTER_VERTICAL);
      row += 1;
    }

    // AF
    {
      const std::array<wxString, 5> af_choices{{_("1x"), _("2x"), _("4x"), _("8x"), _("16x")}};
      szr_enh->Add(new wxStaticText(page_enh, wxID_ANY, _("Anisotropic Filtering:")),
                   wxGBPosition(row, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
      szr_enh->Add(CreateChoice(page_enh, Config::GFX_ENHANCE_MAX_ANISOTROPY,
                                wxGetTranslation(af_desc), af_choices.size(), af_choices.data()),
                   wxGBPosition(row, 1), span2, wxALIGN_CENTER_VERTICAL);
      row += 1;
    }

    // ubershaders
    {
      const std::array<wxString, 3> mode_choices = {{_("Disabled"), _("Hybrid"), _("Exclusive")}};

      wxChoice* const choice_mode =
          new wxChoice(page_enh, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       static_cast<int>(mode_choices.size()), mode_choices.data());
      RegisterControl(choice_mode, wxGetTranslation(ubershader_desc));
      szr_enh->Add(new wxStaticText(page_enh, wxID_ANY, _("Ubershaders:")), wxGBPosition(row, 0),
                   wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
      szr_enh->Add(choice_mode, wxGBPosition(row, 1), span2, wxALIGN_CENTER_VERTICAL);
      row += 1;

      // Determine ubershader mode
      choice_mode->Bind(wxEVT_CHOICE, &VideoConfigDiag::OnUberShaderModeChanged, this);
      if (Config::GetBase(Config::GFX_DISABLE_SPECIALIZED_SHADERS))
        choice_mode->SetSelection(2);
      else if (Config::GetBase(Config::GFX_BACKGROUND_SHADER_COMPILING))
        choice_mode->SetSelection(1);
      else
        choice_mode->SetSelection(0);
    }

    // postproc shader
    if (vconfig.backend_info.bSupportsPostProcessing)
    {
      choice_ppshader = new wxChoice(page_enh, wxID_ANY);
      RegisterControl(choice_ppshader, wxGetTranslation(ppshader_desc));
      button_config_pp = new wxButton(page_enh, wxID_ANY, _("Config"));

      PopulatePostProcessingShaders();

      choice_ppshader->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_PPShader, this);
      button_config_pp->Bind(wxEVT_BUTTON, &VideoConfigDiag::Event_ConfigurePPShader, this);

      szr_enh->Add(new wxStaticText(page_enh, wxID_ANY, _("Post-Processing Effect:")),
                   wxGBPosition(row, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
      szr_enh->Add(choice_ppshader, wxGBPosition(row, 1), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
      szr_enh->Add(button_config_pp, wxGBPosition(row, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
      row += 1;
    }
    else
    {
      choice_ppshader = nullptr;
      button_config_pp = nullptr;
    }

    // Scaled copy, PL, Bilinear filter
    wxGridSizer* const cb_szr = new wxGridSizer(2, space5, space5);
    cb_szr->Add(CreateCheckBox(page_enh, _("Scaled EFB Copy"),
                               wxGetTranslation(scaled_efb_copy_desc),
                               Config::GFX_HACK_COPY_EFB_ENABLED));
    cb_szr->Add(CreateCheckBox(page_enh, _("Per-Pixel Lighting"),
                               wxGetTranslation(pixel_lighting_desc),
                               Config::GFX_ENABLE_PIXEL_LIGHTING));
    cb_szr->Add(CreateCheckBox(page_enh, _("Force Texture Filtering"),
                               wxGetTranslation(force_filtering_desc),
                               Config::GFX_ENHANCE_FORCE_FILTERING));
    cb_szr->Add(CreateCheckBox(page_enh, _("Widescreen Hack"), wxGetTranslation(ws_hack_desc),
                               Config::GFX_WIDESCREEN_HACK));
    cb_szr->Add(CreateCheckBox(page_enh, _("Disable Fog"), wxGetTranslation(disable_fog_desc),
                               Config::GFX_DISABLE_FOG));
    cb_szr->Add(CreateCheckBox(page_enh, _("Force 24-Bit Color"), wxGetTranslation(true_color_desc),
                               Config::GFX_ENHANCE_FORCE_TRUE_COLOR));
    szr_enh->Add(cb_szr, wxGBPosition(row, 0), wxGBSpan(1, 3));
    row += 1;

    wxStaticBoxSizer* const group_enh =
        new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Enhancements"));
    group_enh->Add(szr_enh, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
    group_enh->AddSpacer(space5);

    szr_enh_main->AddSpacer(space5);
    szr_enh_main->Add(group_enh, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);

    // - stereoscopy

    if (vconfig.backend_info.bSupportsGeometryShaders)
    {
      wxFlexGridSizer* const szr_stereo = new wxFlexGridSizer(2, space5, space5);
      szr_stereo->AddGrowableCol(1);

      szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Stereoscopic 3D Mode:")), 0,
                      wxALIGN_CENTER_VERTICAL);

#ifdef _WIN32
      const wxString stereo_choices[] = {_("Off"),      _("Side-by-Side"), _("Top-and-Bottom"),
                                         _("Anaglyph"), _("OSVR"), _("HDMI 3D"), _("Nvidia 3D Vision"),
                                         _("Oculus"),   _("VR920")};
      const wxString stereo_choices_na[] = {_("Off"),      _("Side-by-Side"), _("Top-and-Bottom"),
                                            _("Anaglyph"), _("OSVR"), _("HDMI 3D"),         _("N/A"),
                                            _("Oculus"),   _("VR920")};
#else
      const wxString stereo_choices[] = {_("Off"),      _("Side-by-Side"), _("Top-and-Bottom"),
                                         _("Anaglyph"), _("OSVR"), _("HDMI 3D"),         _("Nvidia 3D Vision"),
                                         _("Oculus")};
      const wxString stereo_choices_na[] = {_("Off"),      _("Side-by-Side"), _("Top-and-Bottom"),
                                            _("Anaglyph"), _("OSVR"), _("HDMI 3D"),         _("N/A"),
                                            _("Oculus")};
#endif
      wxChoice* stereo_choice;
      if (vconfig.backend_info.bSupports3DVision)
        stereo_choice =
            CreateChoice(page_enh, Config::GFX_STEREO_MODE, wxGetTranslation(stereo_3d_desc),
                         (sizeof(stereo_choices) / sizeof(*stereo_choices)), stereo_choices);
      else
        stereo_choice = CreateChoice(
            page_enh, Config::GFX_STEREO_MODE, wxGetTranslation(stereo_3d_desc),
            (sizeof(stereo_choices_na) / sizeof(*stereo_choices_na)), stereo_choices_na);
      stereo_choice->Bind(wxEVT_CHOICE, &VideoConfigDiag::Event_StereoMode, this);
      szr_stereo->Add(stereo_choice, 0, wxALIGN_CENTER_VERTICAL);

      DolphinSlider* const sep_slider =
          new DolphinSlider(page_enh, wxID_ANY, vconfig.iStereoDepth, 0, 100, wxDefaultPosition,
                            FromDIP(wxSize(200, -1)));
      sep_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_StereoDepth, this);
      RegisterControl(sep_slider, wxGetTranslation(stereo_depth_desc));

      szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Separation:")));
      szr_stereo->Add(sep_slider);

      conv_slider =
          new DolphinSlider(page_enh, wxID_ANY, vconfig.iStereoConvergencePercentage, 0, 200,
                            wxDefaultPosition, FromDIP(wxSize(200, -1)), wxSL_AUTOTICKS);
      conv_slider->ClearTicks();
      conv_slider->SetTick(100);
      conv_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_StereoConvergence, this);
      RegisterControl(conv_slider, wxGetTranslation(stereo_convergence_desc));

      szr_stereo->Add(new wxStaticText(page_enh, wxID_ANY, _("Convergence:")));
      szr_stereo->Add(conv_slider);

      szr_stereo->Add(CreateCheckBox(page_enh, _("Swap Eyes"), wxGetTranslation(stereo_swap_desc),
                                     Config::GFX_STEREO_SWAP_EYES));

      wxStaticBoxSizer* const group_stereo =
          new wxStaticBoxSizer(wxVERTICAL, page_enh, _("Stereoscopy"));
      group_stereo->Add(szr_stereo, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_stereo->AddSpacer(space5);

      szr_enh_main->AddSpacer(space5);
      szr_enh_main->Add(group_stereo, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    }

    szr_enh_main->AddSpacer(space5);
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
    wxStaticBoxSizer* const szr_efb =
        new wxStaticBoxSizer(wxVERTICAL, page_hacks, _("Embedded Frame Buffer (EFB)"));

    // EFB copies
    wxStaticBoxSizer* const group_efbcopy =
        new wxStaticBoxSizer(wxHORIZONTAL, page_hacks, _("EFB Copies"));

    SettingCheckBox* efbcopy_disable =
        CreateCheckBox(page_hacks, _("Disable"), wxGetTranslation(efb_copy_desc),
                       Config::GFX_HACK_COPY_EFB_ENABLED, true);

    efbcopy_clear_disable = CreateCheckBox(page_hacks, _("Remove Blank EFB Copy Box"),
                                           wxGetTranslation(efb_copy_clear_desc),
                                           Config::GFX_HACK_EFB_COPY_CLEAR_DISABLE, false);

    efbcopy_texture =
        CreateRadioButton(page_hacks, _("Texture"), wxGetTranslation(skip_efb_copy_to_ram_desc),
                          Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, false, wxRB_GROUP);
    efbcopy_ram = CreateRadioButton(page_hacks, _("RAM"), wxGetTranslation(efb_copy_ram_desc),
                                    Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, true);

    group_efbcopy->Add(efbcopy_disable, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
    group_efbcopy->Add(efbcopy_clear_disable, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
    group_efbcopy->AddStretchSpacer(1);
    group_efbcopy->Add(efbcopy_texture, 0, wxRIGHT, 5);
    group_efbcopy->Add(efbcopy_ram, 0, wxRIGHT, 5);

    szr_efb->Add(CreateCheckBox(page_hacks, _("Skip EFB Access from CPU"),
                                wxGetTranslation(efb_access_desc),
                                Config::GFX_HACK_EFB_ACCESS_ENABLE, true),
                 0, wxLEFT | wxRIGHT, space5);
    szr_efb->AddSpacer(space5);
    szr_efb->Add(CreateCheckBox(page_hacks, _("Ignore Format Changes"),
                                wxGetTranslation(efb_emulate_format_changes_desc),
                                Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES, true),
                 0, wxLEFT | wxRIGHT, space5);
    szr_efb->AddSpacer(space5);
    szr_efb->Add(group_efbcopy, 0, wxEXPAND | wxALL, 5);
    // szr_efb->Add(CreateCheckBox(page_hacks, _("Store EFB Copies to Texture Only"),
    //                             wxGetTranslation(skip_efb_copy_to_ram_desc),
    //                             Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM),
    //              0, wxLEFT | wxRIGHT, space5);

    szr_hacks->AddSpacer(space5);
    szr_hacks->Add(szr_efb, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);

    // Texture cache
    {
      wxStaticBoxSizer* const szr_safetex =
          new wxStaticBoxSizer(wxVERTICAL, page_hacks, _("Texture Cache"));

      int slider_pos = -1;
      if (vconfig.iSafeTextureCache_ColorSamples == 0)
        slider_pos = 0;
      else if (vconfig.iSafeTextureCache_ColorSamples == 512)
        slider_pos = 1;
      else if (vconfig.iSafeTextureCache_ColorSamples == 128)
        slider_pos = 2;

      DolphinSlider* const stc_slider =
          new DolphinSlider(page_hacks, wxID_ANY, std::max(slider_pos, 0), 0, 2, wxDefaultPosition,
                            wxDefaultSize, wxSL_HORIZONTAL | wxSL_BOTTOM);
      stc_slider->Bind(wxEVT_SLIDER, &VideoConfigDiag::Event_SafeTextureCache, this);
      RegisterControl(stc_slider, wxGetTranslation(stc_desc));

      wxBoxSizer* const slide_szr = new wxBoxSizer(wxHORIZONTAL);
      slide_szr->Add(new wxStaticText(page_hacks, wxID_ANY, _("Accuracy:")), 0,
                     wxALIGN_CENTER_VERTICAL);
      slide_szr->AddStretchSpacer(1);
      slide_szr->Add(new wxStaticText(page_hacks, wxID_ANY, _("Safe")), 0,
                     wxALIGN_CENTER_VERTICAL | wxLEFT, space5);
      slide_szr->Add(stc_slider, 2, wxALIGN_CENTER_VERTICAL);
      slide_szr->Add(new wxStaticText(page_hacks, wxID_ANY, _("Fast")), 0, wxALIGN_CENTER_VERTICAL);

      szr_safetex->Add(slide_szr, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);

      if (vconfig.backend_info.bSupportsGPUTextureDecoding)
      {
        szr_safetex->Add(CreateCheckBox(page_hacks, _("GPU Texture Decoding"),
                                        wxGetTranslation(gpu_texture_decoding_desc),
                                        Config::GFX_ENABLE_GPU_TEXTURE_DECODING),
                         1, wxEXPAND | wxLEFT | wxRIGHT, space5);
      }

      if (slider_pos == -1)
      {
        stc_slider->Disable();
        wxString msg = wxString::Format(_("Hash tap count is set to %d which is non-standard.\n"
                                          "You will need to edit the INI manually."),
                                        vconfig.iSafeTextureCache_ColorSamples);
        szr_safetex->AddSpacer(space5);
        szr_safetex->Add(new wxStaticText(page_hacks, wxID_ANY, msg), 0,
                         wxALIGN_RIGHT | wxLEFT | wxRIGHT, space5);
      }
      szr_safetex->AddSpacer(space5);

      szr_hacks->AddSpacer(space5);
      szr_hacks->Add(szr_safetex, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    }

    // - XFB
    {
      wxStaticBoxSizer* const group_xfb =
          new wxStaticBoxSizer(wxVERTICAL, page_hacks, _("External Frame Buffer (XFB)"));

      SettingCheckBox* disable_xfb = CreateCheckBox(
          page_hacks, _("Disable"), wxGetTranslation(xfb_desc), Config::GFX_USE_XFB, true);
      virtual_xfb = CreateRadioButton(page_hacks, _("Virtual"), wxGetTranslation(xfb_virtual_desc),
                                      Config::GFX_USE_REAL_XFB, true, wxRB_GROUP);
      real_xfb = CreateRadioButton(page_hacks, _("Real"), wxGetTranslation(xfb_real_desc),
                                   Config::GFX_USE_REAL_XFB);

      wxBoxSizer* const szr = new wxBoxSizer(wxHORIZONTAL);
      szr->Add(disable_xfb, 0, wxALIGN_CENTER_VERTICAL);
      szr->AddStretchSpacer(1);
      szr->Add(virtual_xfb, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space5);
      szr->Add(real_xfb, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, space5);

      group_xfb->Add(szr, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_xfb->AddSpacer(space5);

      szr_hacks->AddSpacer(space5);
      szr_hacks->Add(group_xfb, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    }  // xfb

    // - other hacks
    {
      wxGridSizer* const szr_other = new wxGridSizer(2, space5, space5);
      szr_other->Add(CreateCheckBox(page_hacks, _("Fast Depth Calculation"),
                                    wxGetTranslation(fast_depth_calc_desc),
                                    Config::GFX_FAST_DEPTH_CALC));
      szr_other->Add(CreateCheckBox(page_hacks, _("Disable Bounding Box"),
                                    wxGetTranslation(disable_bbox_desc),
                                    Config::GFX_HACK_BBOX_ENABLE, true));
      vertex_rounding_checkbox =
          CreateCheckBox(page_hacks, _("Vertex Rounding"), wxGetTranslation(vertex_rounding_desc),
                         Config::GFX_HACK_VERTEX_ROUDING);
      szr_other->Add(vertex_rounding_checkbox);

      wxStaticBoxSizer* const group_other =
          new wxStaticBoxSizer(wxVERTICAL, page_hacks, _("Other"));
      group_other->Add(szr_other, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_other->AddSpacer(space5);

      szr_hacks->AddSpacer(space5);
      szr_hacks->Add(group_other, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    }

    szr_hacks->AddSpacer(space5);
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
      wxGridSizer* const szr_debug = new wxGridSizer(2, space5, space5);

      szr_debug->Add(CreateCheckBox(page_advanced, _("Enable Wireframe"),
                                    wxGetTranslation(wireframe_desc),
                                    Config::GFX_ENABLE_WIREFRAME));
      szr_debug->Add(CreateCheckBox(page_advanced, _("Show Statistics"),
                                    wxGetTranslation(show_stats_desc), Config::GFX_OVERLAY_STATS));
      szr_debug->Add(CreateCheckBox(page_advanced, _("Texture Format Overlay"),
                                    wxGetTranslation(texfmt_desc),
                                    Config::GFX_TEXFMT_OVERLAY_ENABLE));
      szr_debug->Add(CreateCheckBox(page_advanced, _("Enable API Validation Layers"),
                                    wxGetTranslation(validation_layer_desc),
                                    Config::GFX_ENABLE_VALIDATION_LAYER));

      wxStaticBoxSizer* const group_debug =
          new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Debugging"));
      group_debug->Add(szr_debug, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_debug->AddSpacer(space5);

      szr_advanced->AddSpacer(space5);
      szr_advanced->Add(group_debug, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    }

    // - utility
    {
      wxGridSizer* const szr_utility = new wxGridSizer(2, space5, space5);

      szr_utility->Add(CreateCheckBox(page_advanced, _("Dump Textures"),
                                      wxGetTranslation(dump_textures_desc),
                                      Config::GFX_DUMP_TEXTURES));
      szr_utility->Add(CreateCheckBox(page_advanced, _("Load Custom Textures"),
                                      wxGetTranslation(load_hires_textures_desc),
                                      Config::GFX_HIRES_TEXTURES));
      cache_hires_textures = CreateCheckBox(page_advanced, _("Prefetch Custom Textures"),
                                            wxGetTranslation(cache_hires_textures_desc),
                                            Config::GFX_CACHE_HIRES_TEXTURES);
      szr_utility->Add(cache_hires_textures);

      if (vconfig.backend_info.bSupportsInternalResolutionFrameDumps)
      {
        szr_utility->Add(CreateCheckBox(page_advanced, _("Full Resolution Frame Dumps"),
                                        wxGetTranslation(internal_resolution_frame_dumping_desc),
                                        Config::GFX_INTERNAL_RESOLUTION_FRAME_DUMPS));
      }

      szr_utility->Add(CreateCheckBox(page_advanced, _("Dump EFB Target"),
                                      wxGetTranslation(dump_efb_desc),
                                      Config::GFX_DUMP_EFB_TARGET));
      szr_utility->Add(CreateCheckBox(page_advanced, _("Mouse Free Look"),
                                      wxGetTranslation(free_look_desc), Config::GFX_FREE_LOOK));
#if defined(HAVE_FFMPEG)
      szr_utility->Add(CreateCheckBox(page_advanced, _("Frame Dumps Use FFV1"),
                                      wxGetTranslation(use_ffv1_desc), Config::GFX_USE_FFV1));
#endif

      wxStaticBoxSizer* const group_utility =
          new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Utility"));
      group_utility->Add(szr_utility, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_utility->AddSpacer(space5);

      szr_advanced->AddSpacer(space5);
      szr_advanced->Add(group_utility, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    }

    // - misc
    {
      wxGridSizer* const szr_misc = new wxGridSizer(2, space5, space5);

      szr_misc->Add(
          CreateCheckBox(page_advanced, _("Crop"), wxGetTranslation(crop_desc), Config::GFX_CROP));

      // Progressive Scan
      {
        progressive_scan_checkbox =
            new wxCheckBox(page_advanced, wxID_ANY, _("Enable Progressive Scan"));
        RegisterControl(progressive_scan_checkbox, wxGetTranslation(prog_scan_desc));
        progressive_scan_checkbox->Bind(wxEVT_CHECKBOX, &VideoConfigDiag::Event_ProgressiveScan,
                                        this);

        // TODO: split this into two different settings, one for Wii and one for GC?
        progressive_scan_checkbox->SetValue(Config::Get(Config::SYSCONF_PROGRESSIVE_SCAN));
        szr_misc->Add(progressive_scan_checkbox);
      }

#if defined WIN32
      // Borderless Fullscreen
      borderless_fullscreen = CreateCheckBox(page_advanced, _("Borderless Fullscreen"),
                                             wxGetTranslation(borderless_fullscreen_desc),
                                             Config::GFX_BORDERLESS_FULLSCREEN);
      szr_misc->Add(borderless_fullscreen);
#endif

      wxStaticBoxSizer* const group_misc =
          new wxStaticBoxSizer(wxVERTICAL, page_advanced, _("Misc"));
      group_misc->Add(szr_misc, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
      group_misc->AddSpacer(space5);

      szr_advanced->AddSpacer(space5);
      szr_advanced->Add(group_misc, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
    }

    szr_advanced->AddSpacer(space5);
    szr_advanced->AddStretchSpacer();
    CreateDescriptionArea(page_advanced, szr_advanced);
    page_advanced->SetSizerAndFit(szr_advanced);
  }

  wxStdDialogButtonSizer* btn_sizer = CreateStdDialogButtonSizer(wxOK | wxNO_DEFAULT);
  btn_sizer->GetAffirmativeButton()->SetLabel(_("Close"));
  SetEscapeId(wxID_OK);  // Escape key or window manager 'X'

  Bind(wxEVT_BUTTON, &VideoConfigDiag::Event_Close, this, wxID_OK);

  wxBoxSizer* const szr_main = new wxBoxSizer(wxVERTICAL);
  szr_main->AddSpacer(space5);
  szr_main->Add(notebook, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);
  szr_main->Add(btn_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  szr_main->AddSpacer(space5);

  SetLayoutAdaptationMode(wxDIALOG_ADAPTATION_MODE_ENABLED);
  SetLayoutAdaptationLevel(wxDIALOG_ADAPTATION_STANDARD_SIZER);
  SetSizerAndFit(szr_main);
  Center();
  SetFocus();

  UpdateWindowUI();
}

void VideoConfigDiag::Event_Backend(wxCommandEvent& ev)
{
  auto& new_backend = g_available_video_backends[ev.GetInt()];

  if (g_video_backend != new_backend.get())
  {
    bool do_switch = !Core::IsRunning();
    if (new_backend->GetName() == "Software Renderer")
    {
      do_switch =
          (wxYES ==
           wxMessageBox(_("Software rendering is an order of magnitude slower than using the "
                          "other backends.\nIt's only useful for debugging purposes.\nDo you "
                          "really want to enable software rendering? If unsure, select 'No'."),
                        _("Warning"), wxYES_NO | wxNO_DEFAULT | wxICON_EXCLAMATION, this));
    }

    if (do_switch)
    {
      // TODO: Only reopen the dialog if the software backend is
      // selected (make sure to reinitialize backend info)
      // reopen the dialog
      Close();

      g_video_backend = new_backend.get();
      SConfig::GetInstance().m_strVideoBackend = g_video_backend->GetName();

      g_video_backend->ShowConfig(GetParent());
    }
    else
    {
      // Select current backend again
      choice_backend->SetStringSelection(StrToWxStr(g_video_backend->GetName()));
    }
  }

  ev.Skip();
}

void VideoConfigDiag::Event_DisplayResolution(wxCommandEvent& ev)
{
  // "Auto" has been translated, it needs to be the English string "Auto" to work
  switch (choice_display_resolution->GetSelection())
  {
  case 0:
    SConfig::GetInstance().strFullscreenResolution = "Auto";
    break;
  case wxNOT_FOUND:
    break;  // Nothing is selected.
  default:
    SConfig::GetInstance().strFullscreenResolution =
        WxStrToStr(choice_display_resolution->GetStringSelection());
  }
#if defined(HAVE_XRANDR) && HAVE_XRANDR
  main_frame->m_xrr_config->Update();
#endif
  ev.Skip();
}

void VideoConfigDiag::Event_ProgressiveScan(wxCommandEvent& ev)
{
  Config::SetBase(Config::SYSCONF_PROGRESSIVE_SCAN, ev.IsChecked());
  ev.Skip();
}

void VideoConfigDiag::Event_SafeTextureCache(wxCommandEvent& ev)
{
  int samples[] = {0, 512, 128};
  Config::SetBaseOrCurrent(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES, samples[ev.GetInt()]);

  ev.Skip();
}

void VideoConfigDiag::Event_PPShader(wxCommandEvent& ev)
{
  const int sel = ev.GetInt();
  std::string shader = sel ? WxStrToStr(ev.GetString()) : "";
  Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER, shader);

  // Should we enable the configuration button?
  PostProcessingShaderConfiguration postprocessing_shader;
  postprocessing_shader.LoadShader(shader);
  button_config_pp->Enable(postprocessing_shader.HasOptions());

  ev.Skip();
}

void VideoConfigDiag::Event_ConfigurePPShader(wxCommandEvent& ev)
{
  PostProcessingConfigDiag dialog(this, vconfig.sPostProcessingShader);
  dialog.ShowModal();

  ev.Skip();
}

void VideoConfigDiag::Event_StereoDepth(wxCommandEvent& ev)
{
  Config::SetBaseOrCurrent(Config::GFX_STEREO_DEPTH, ev.GetInt());

  ev.Skip();
}

void VideoConfigDiag::Event_StereoConvergence(wxCommandEvent& ev)
{
  // Snap the slider
  int value = ev.GetInt();
  if (90 < value && value < 110)
    conv_slider->SetValue(100);

  Config::SetBaseOrCurrent(Config::GFX_STEREO_CONVERGENCE_PERCENTAGE, conv_slider->GetValue());

  ev.Skip();
}

void VideoConfigDiag::Event_StereoMode(wxCommandEvent& ev)
{
  if (vconfig.backend_info.bSupportsPostProcessing)
  {
    // Anaglyph overrides post-processing shaders
    choice_ppshader->Clear();
  }

  ev.Skip();
}

void VideoConfigDiag::Event_Close(wxCommandEvent& ev)
{
  Config::Save();
  ev.Skip();
}

void VideoConfigDiag::OnUpdateUI(wxUpdateUIEvent& ev)
{
  // Anti-aliasing
  choice_aamode->Enable(vconfig.backend_info.AAModes.size() > 1);
  text_aamode->Enable(vconfig.backend_info.AAModes.size() > 1);

  // XFB
  virtual_xfb->Enable(vconfig.bUseXFB);
  real_xfb->Enable(vconfig.bUseXFB);

  // custom textures
  cache_hires_textures->Enable(vconfig.bHiresTextures);

  // Vertex rounding
  vertex_rounding_checkbox->Enable(vconfig.iEFBScale != SCALE_1X);

  // Repopulating the post-processing shaders can't be done from an event
  if (choice_ppshader && choice_ppshader->IsEmpty())
    PopulatePostProcessingShaders();

  // Things which shouldn't be changed during emulation
  if (Core::IsRunning())
  {
    choice_backend->Disable();
    label_backend->Disable();

    // D3D only
    if (vconfig.backend_info.Adapters.size())
    {
      choice_adapter->Disable();
      label_adapter->Disable();
    }

#ifndef __APPLE__
    // This isn't supported on OS X.

    choice_display_resolution->Disable();
    label_display_resolution->Disable();
#endif

    progressive_scan_checkbox->Disable();
    render_to_main_checkbox->Disable();
  }

  ev.Skip();
}

SettingCheckBox* VideoConfigDiag::CreateCheckBox(wxWindow* parent, const wxString& label,
                                                 const wxString& description,
                                                 const Config::ConfigInfo<bool>& setting,
                                                 bool reverse, long style)
{
  SettingCheckBox* const cb =
      new SettingCheckBox(parent, label, wxString(), setting, reverse, style);
  RegisterControl(cb, description);
  return cb;
}

RefBoolSetting<wxCheckBox>* VideoConfigDiag::CreateCheckBoxRefBool(wxWindow* parent,
                                                                   const wxString& label,
                                                                   const wxString& description,
                                                                   bool& setting)
{
  auto* const cb = new RefBoolSetting<wxCheckBox>(parent, label, wxString(), setting, false, 0);
  RegisterControl(cb, description);
  return cb;
}

SettingChoice* VideoConfigDiag::CreateChoice(wxWindow* parent,
                                             const Config::ConfigInfo<int>& setting,
                                             const wxString& description, int num,
                                             const wxString choices[], long style)
{
  SettingChoice* const ch = new SettingChoice(parent, setting, wxString(), num, choices, style);
  RegisterControl(ch, description);
  return ch;
}

SettingRadioButton* VideoConfigDiag::CreateRadioButton(wxWindow* parent, const wxString& label,
                                                       const wxString& description,
                                                       const Config::ConfigInfo<bool>& setting,
                                                       bool reverse, long style)
{
  SettingRadioButton* const rb =
      new SettingRadioButton(parent, label, wxString(), setting, reverse, style);
  RegisterControl(rb, description);
  return rb;
}

SettingNumber* VideoConfigDiag::CreateNumber(wxWindow* parent,
                                             const Config::ConfigInfo<float>& setting,
                                             const wxString& description, float min, float max,
                                             float inc, long style)
{
  SettingNumber* const sn = new SettingNumber(parent, wxString(), setting, min, max, inc, style);
  RegisterControl(sn, description);
  return sn;
}

/* Use this to register descriptions for controls which have NOT been created using the Create*
 * functions from above */
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
  if (!ctrl)
    return;

  // look up description text object from the control's parent (which is the wxPanel of the current
  // tab)
  wxStaticText* descr_text = desc_texts[ctrl->GetParent()];
  if (!descr_text)
    return;

  // look up the description of the selected control and assign it to the current description text
  // object's label
  descr_text->SetLabel(ctrl_descs[ctrl]);
  descr_text->Wrap(descr_text->GetSize().GetWidth());

  ev.Skip();
}

void VideoConfigDiag::Evt_LeaveControl(wxMouseEvent& ev)
{
  // look up description text control and reset its label
  wxWindow* ctrl = static_cast<wxWindow*>(ev.GetEventObject());
  if (!ctrl)
    return;
  wxStaticText* descr_text = desc_texts[ctrl->GetParent()];
  if (!descr_text)
    return;

  descr_text->SetLabel(wxGetTranslation(default_desc));
  descr_text->Wrap(descr_text->GetSize().GetWidth());

  ev.Skip();
}

void VideoConfigDiag::CreateDescriptionArea(wxPanel* const page, wxBoxSizer* const sizer)
{
  const int space5 = FromDIP(5);

  // Create description frame
  wxStaticBoxSizer* const desc_sizer = new wxStaticBoxSizer(wxVERTICAL, page, _("Description"));
  sizer->Add(desc_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  sizer->AddSpacer(space5);

  // Create description text (220 = 75*4 (75 chars), 80 = 10*8 (10 lines))
  wxStaticText* const desc_text =
      new wxStaticText(page, wxID_ANY, wxGetTranslation(default_desc), wxDefaultPosition,
                       wxDLG_UNIT(this, wxSize(220, 80)), wxST_NO_AUTORESIZE);
  desc_text->Wrap(desc_text->GetMinWidth());
  desc_sizer->Add(desc_text, 1, wxEXPAND | wxLEFT | wxRIGHT, space5);
  desc_sizer->AddSpacer(space5);

  // Store description text object for later lookup
  desc_texts.emplace(page, desc_text);
}

void VideoConfigDiag::PopulatePostProcessingShaders()
{
  std::vector<std::string> shaders =
      vconfig.iStereoMode == STEREO_ANAGLYPH ?
          PostProcessingShaderImplementation::GetAnaglyphShaderList(vconfig.backend_info.api_type) :
          PostProcessingShaderImplementation::GetShaderList(vconfig.backend_info.api_type);

  if (shaders.empty())
    return;

  choice_ppshader->AppendString(_("Off"));

  for (const std::string& shader : shaders)
  {
    choice_ppshader->AppendString(StrToWxStr(shader));
  }

  if (vconfig.sPostProcessingShader.empty())
  {
    // Select (off) value in choice.
    choice_ppshader->Select(0);
  }
  else if (!choice_ppshader->SetStringSelection(StrToWxStr(vconfig.sPostProcessingShader)))
  {
    // Invalid shader, reset it to default
    choice_ppshader->Select(0);

    if (vconfig.iStereoMode == STEREO_ANAGLYPH)
    {
      Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER, std::string("dubois"));
      choice_ppshader->SetStringSelection(StrToWxStr(vconfig.sPostProcessingShader));
    }
    else
      Config::SetBaseOrCurrent(Config::GFX_ENHANCE_POST_SHADER, std::string(""));
  }

  // Should the configuration button be loaded by default?
  PostProcessingShaderConfiguration postprocessing_shader;
  postprocessing_shader.LoadShader(vconfig.sPostProcessingShader);
  button_config_pp->Enable(postprocessing_shader.HasOptions());
}

void VideoConfigDiag::PopulateAAList()
{
  const auto& aa_modes = vconfig.backend_info.AAModes;
  const bool supports_ssaa = vconfig.backend_info.bSupportsSSAA;
  m_msaa_modes = 0;

  for (auto mode : aa_modes)
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
    for (auto mode : aa_modes)
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

template class FloatSetting<float>;
template class FloatSetting<double>;

void VideoConfigDiag::OnAAChanged(wxCommandEvent& ev)
{
  size_t mode = ev.GetInt();
  ev.Skip();

  Config::SetBaseOrCurrent(Config::GFX_SSAA, mode > m_msaa_modes);
  mode -= vconfig.bSSAA * m_msaa_modes;

  if (mode >= vconfig.backend_info.AAModes.size())
    return;

  Config::SetBaseOrCurrent(Config::GFX_MSAA, vconfig.backend_info.AAModes[mode]);
}

void VideoConfigDiag::OnUberShaderModeChanged(wxCommandEvent& ev)
{
  // 0: No ubershaders
  // 1: Hybrid ubershaders
  // 2: Only ubershaders
  int mode = ev.GetInt();
  Config::SetBaseOrCurrent(Config::GFX_BACKGROUND_SHADER_COMPILING, mode == 1);
  Config::SetBaseOrCurrent(Config::GFX_DISABLE_SPECIALIZED_SHADERS, mode == 2);
}
