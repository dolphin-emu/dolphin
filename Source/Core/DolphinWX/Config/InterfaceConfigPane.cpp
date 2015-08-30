// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/language.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/HotkeyManager.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/InputConfigDiag.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Config/InterfaceConfigPane.h"

#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include "DolphinWX/X11Utils.h"
#endif

static const wxLanguage language_ids[] =
{
	wxLANGUAGE_DEFAULT,

	wxLANGUAGE_CATALAN,
	wxLANGUAGE_CZECH,
	wxLANGUAGE_GERMAN,
	wxLANGUAGE_ENGLISH,
	wxLANGUAGE_SPANISH,
	wxLANGUAGE_FRENCH,
	wxLANGUAGE_ITALIAN,
	wxLANGUAGE_HUNGARIAN,
	wxLANGUAGE_DUTCH,
	wxLANGUAGE_NORWEGIAN_BOKMAL,
	wxLANGUAGE_POLISH,
	wxLANGUAGE_PORTUGUESE,
	wxLANGUAGE_PORTUGUESE_BRAZILIAN,
	wxLANGUAGE_SERBIAN,
	wxLANGUAGE_SWEDISH,
	wxLANGUAGE_TURKISH,

	wxLANGUAGE_GREEK,
	wxLANGUAGE_RUSSIAN,
	wxLANGUAGE_HEBREW,
	wxLANGUAGE_ARABIC,
	wxLANGUAGE_FARSI,
	wxLANGUAGE_KOREAN,
	wxLANGUAGE_JAPANESE,
	wxLANGUAGE_CHINESE_SIMPLIFIED,
	wxLANGUAGE_CHINESE_TRADITIONAL,
};

InterfaceConfigPane::InterfaceConfigPane(wxWindow* parent, wxWindowID id)
	: wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
}

void InterfaceConfigPane::InitializeGUI()
{
	// GUI language arrayStrings
	// keep these in sync with the langIds array at the beginning of this file
	m_interface_lang_strings.Add(_("<System Language>"));
	m_interface_lang_strings.Add(L"Catal\u00E0");                                       // Catalan
	m_interface_lang_strings.Add(L"\u010Ce\u0161tina");                                 // Czech
	m_interface_lang_strings.Add(L"Deutsch");                                           // German
	m_interface_lang_strings.Add(L"English");                                           // English
	m_interface_lang_strings.Add(L"Espa\u00F1ol");                                      // Spanish
	m_interface_lang_strings.Add(L"Fran\u00E7ais");                                     // French
	m_interface_lang_strings.Add(L"Italiano");                                          // Italian
	m_interface_lang_strings.Add(L"Magyar");                                            // Hungarian
	m_interface_lang_strings.Add(L"Nederlands");                                        // Dutch
	m_interface_lang_strings.Add(L"Norsk bokm\u00E5l");                                 // Norwegian
	m_interface_lang_strings.Add(L"Polski");                                            // Polish
	m_interface_lang_strings.Add(L"Portugu\u00EAs");                                    // Portuguese
	m_interface_lang_strings.Add(L"Portugu\u00EAs (Brasil)");                           // Portuguese (Brazil)
	m_interface_lang_strings.Add(L"Srpski");                                            // Serbian
	m_interface_lang_strings.Add(L"Svenska");                                           // Swedish
	m_interface_lang_strings.Add(L"T\u00FCrk\u00E7e");                                  // Turkish
	m_interface_lang_strings.Add(L"\u0395\u03BB\u03BB\u03B7\u03BD\u03B9\u03BA\u03AC");  // Greek
	m_interface_lang_strings.Add(L"\u0420\u0443\u0441\u0441\u043A\u0438\u0439");        // Russian
	m_interface_lang_strings.Add(L"\u05E2\u05D1\u05E8\u05D9\u05EA");                    // Hebrew
	m_interface_lang_strings.Add(L"\u0627\u0644\u0639\u0631\u0628\u064A\u0629");        // Arabic
	m_interface_lang_strings.Add(L"\u0641\u0627\u0631\u0633\u06CC");                    // Farsi
	m_interface_lang_strings.Add(L"\uD55C\uAD6D\uC5B4");                                // Korean
	m_interface_lang_strings.Add(L"\u65E5\u672C\u8A9E");                                // Japanese
	m_interface_lang_strings.Add(L"\u7B80\u4F53\u4E2D\u6587");                          // Simplified Chinese
	m_interface_lang_strings.Add(L"\u7E41\u9AD4\u4E2D\u6587");                          // Traditional Chinese

	m_confirm_stop_checkbox     = new wxCheckBox(this, wxID_ANY, _("Confirm on Stop"));
	m_panic_handlers_checkbox   = new wxCheckBox(this, wxID_ANY, _("Use Panic Handlers"));
	m_osd_messages_checkbox     = new wxCheckBox(this, wxID_ANY, _("On-Screen Display Messages"));
	m_pause_focus_lost_checkbox = new wxCheckBox(this, wxID_ANY, _("Pause on Focus Lost"));
	m_interface_lang_choice     = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_interface_lang_strings);
	m_hotkey_config_button      = new wxButton(this, wxID_ANY, _("Hotkeys"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	m_theme_choice              = new wxChoice(this, wxID_ANY);

	m_confirm_stop_checkbox->Bind(wxEVT_CHECKBOX, &InterfaceConfigPane::OnConfirmStopCheckBoxChanged, this);
	m_panic_handlers_checkbox->Bind(wxEVT_CHECKBOX, &InterfaceConfigPane::OnPanicHandlersCheckBoxChanged, this);
	m_osd_messages_checkbox->Bind(wxEVT_CHECKBOX, &InterfaceConfigPane::OnOSDMessagesCheckBoxChanged, this);
	m_pause_focus_lost_checkbox->Bind(wxEVT_CHECKBOX, &InterfaceConfigPane::OnPauseOnFocusLostCheckBoxChanged, this);
	m_interface_lang_choice->Bind(wxEVT_CHOICE, &InterfaceConfigPane::OnInterfaceLanguageChoiceChanged, this);
	m_theme_choice->Bind(wxEVT_CHOICE, &InterfaceConfigPane::OnThemeSelected, this);

	m_confirm_stop_checkbox->SetToolTip(_("Show a confirmation box before stopping a game."));
	m_panic_handlers_checkbox->SetToolTip(_("Show a message box when a potentially serious error has occurred.\nDisabling this may avoid annoying and non-fatal messages, but it may result in major crashes having no explanation at all."));
	m_osd_messages_checkbox->SetToolTip(_("Display messages over the emulation screen area.\nThese messages include memory card writes, video backend and CPU information, and JIT cache clearing."));
	m_pause_focus_lost_checkbox->SetToolTip(_("Pauses the emulator when focus is taken away from the emulation window."));
	m_interface_lang_choice->SetToolTip(_("Change the language of the user interface.\nRequires restart."));

	wxBoxSizer* const language_sizer = new wxBoxSizer(wxHORIZONTAL);
	language_sizer->Add(new wxStaticText(this, wxID_ANY, _("Language:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	language_sizer->Add(m_interface_lang_choice, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	language_sizer->AddStretchSpacer();
	language_sizer->Add(m_hotkey_config_button, 0, wxALIGN_RIGHT | wxALL, 5);

	wxBoxSizer* const theme_sizer = new wxBoxSizer(wxHORIZONTAL);
	theme_sizer->Add(new wxStaticText(this, wxID_ANY, _("Theme:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	theme_sizer->Add(m_theme_choice, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	theme_sizer->AddStretchSpacer();

	wxStaticBoxSizer* const main_static_box_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Interface Settings"));
	main_static_box_sizer->Add(m_confirm_stop_checkbox, 0, wxALL, 5);
	main_static_box_sizer->Add(m_panic_handlers_checkbox, 0, wxALL, 5);
	main_static_box_sizer->Add(m_osd_messages_checkbox, 0, wxALL, 5);
	main_static_box_sizer->Add(m_pause_focus_lost_checkbox, 0, wxALL, 5);
	main_static_box_sizer->Add(theme_sizer, 0, wxEXPAND | wxALL, 5);
	main_static_box_sizer->Add(language_sizer, 0, wxEXPAND | wxALL, 5);

	wxBoxSizer* const main_box_sizer = new wxBoxSizer(wxVERTICAL);
	main_box_sizer->Add(main_static_box_sizer, 0, wxEXPAND | wxALL, 5);

	SetSizer(main_box_sizer);
}

void InterfaceConfigPane::LoadGUIValues()
{
	const SCoreStartupParameter& startup_params = SConfig::GetInstance().m_LocalCoreStartupParameter;

	m_confirm_stop_checkbox->SetValue(startup_params.bConfirmStop);
	m_panic_handlers_checkbox->SetValue(startup_params.bUsePanicHandlers);
	m_osd_messages_checkbox->SetValue(startup_params.bOnScreenDisplayMessages);
	m_pause_focus_lost_checkbox->SetValue(SConfig::GetInstance().m_PauseOnFocusLost);

	for (size_t i = 0; i < sizeof(language_ids) / sizeof(wxLanguage); i++)
	{
		if (language_ids [i] == SConfig::GetInstance().m_InterfaceLanguage)
		{
			m_interface_lang_choice->SetSelection(i);
			break;
		}
	}

	LoadThemes();
}

void InterfaceConfigPane::LoadThemes()
{
	CFileSearch::XStringVector theme_dirs;
	theme_dirs.push_back(File::GetUserPath(D_THEMES_IDX));
	theme_dirs.push_back(File::GetSysDirectory() + THEMES_DIR);

	CFileSearch cfs(CFileSearch::XStringVector(1, "*"), theme_dirs);
	auto const& sv = cfs.GetFileNames();
	for (const std::string& filename : sv)
	{
		std::string name, ext;
		SplitPath(filename, nullptr, &name, &ext);

		name += ext;
		const wxString wxname = StrToWxStr(name);
		if (-1 == m_theme_choice->FindString(wxname))
			m_theme_choice->Append(wxname);
	}

	m_theme_choice->SetStringSelection(StrToWxStr(SConfig::GetInstance().m_LocalCoreStartupParameter.theme_name));
}

void InterfaceConfigPane::OnHotkeyConfigButtonClicked(wxCommandEvent& event)
{
	bool was_init = false;

	InputConfig* const hotkey_plugin = HotkeyManagerEmu::GetConfig();

	// check if game is running
	if (g_controller_interface.IsInit())
	{
		was_init = true;
	}
	else
	{
#if defined(HAVE_X11) && HAVE_X11
		Window win = X11Utils::XWindowFromHandle(GetHandle());
		HotkeyManagerEmu::Initialize(reinterpret_cast<void*>(win));
#else
		HotkeyManagerEmu::Initialize(reinterpret_cast<void*>(GetHandle()));
#endif
	}

	InputConfigDialog m_ConfigFrame(this, *hotkey_plugin, _("Dolphin Hotkeys"));
	m_ConfigFrame.ShowModal();

	// if game isn't running
	if (!was_init)
		HotkeyManagerEmu::Shutdown();

	// Update the GUI in case menu accelerators were changed
	main_frame->UpdateGUI();
}

void InterfaceConfigPane::OnConfirmStopCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bConfirmStop = m_confirm_stop_checkbox->IsChecked();
}

void InterfaceConfigPane::OnPanicHandlersCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bUsePanicHandlers = m_panic_handlers_checkbox->IsChecked();
	SetEnableAlert(m_panic_handlers_checkbox->IsChecked());
}

void InterfaceConfigPane::OnOSDMessagesCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.bOnScreenDisplayMessages = m_osd_messages_checkbox->IsChecked();
}

void InterfaceConfigPane::OnInterfaceLanguageChoiceChanged(wxCommandEvent& event)
{
	if (SConfig::GetInstance().m_InterfaceLanguage != language_ids[m_interface_lang_choice->GetSelection()])
		SuccessAlertT("You must restart Dolphin in order for the change to take effect.");

	SConfig::GetInstance().m_InterfaceLanguage = language_ids[m_interface_lang_choice->GetSelection()];
}

void InterfaceConfigPane::OnPauseOnFocusLostCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_PauseOnFocusLost = m_pause_focus_lost_checkbox->IsChecked();
}

void InterfaceConfigPane::OnThemeSelected(wxCommandEvent& event)
{
	SConfig::GetInstance().m_LocalCoreStartupParameter.theme_name = WxStrToStr(m_theme_choice->GetStringSelection());

	main_frame->InitBitmaps();
	main_frame->UpdateGameList();
}

