// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/Config/InterfaceConfigPane.h"

#include <array>
#include <limits>
#include <string>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/gbsizer.h>
#include <wx/language.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "DolphinWX/Frame.h"
#include "DolphinWX/Input/InputConfigDiag.h"
#include "DolphinWX/WxUtils.h"

#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include "UICommon/X11Utils.h"
#endif

static const std::array<std::string, 29> language_ids{{
    "",

    "ms", "ca", "cs",    "da", "de", "en", "es",    "fr",    "hr", "it", "hu", "nl",
    "nb",  // wxWidgets won't accept "no"
    "pl", "pt", "pt_BR", "ro", "sr", "sv", "tr",

    "el", "ru", "ar",    "fa", "ko", "ja", "zh_CN", "zh_TW",
}};

InterfaceConfigPane::InterfaceConfigPane(wxWindow* parent, wxWindowID id) : wxPanel(parent, id)
{
  InitializeGUI();
  LoadGUIValues();
}

void InterfaceConfigPane::InitializeGUI()
{
  // GUI language arrayStrings
  // keep these in sync with the language_ids array at the beginning of this file
  m_interface_lang_strings.Add(_("<System Language>"));

  m_interface_lang_strings.Add(L"Bahasa Melayu");            // Malay
  m_interface_lang_strings.Add(L"Catal\u00E0");              // Catalan
  m_interface_lang_strings.Add(L"\u010Ce\u0161tina");        // Czech
  m_interface_lang_strings.Add(L"Dansk");                    // Danish
  m_interface_lang_strings.Add(L"Deutsch");                  // German
  m_interface_lang_strings.Add(L"English");                  // English
  m_interface_lang_strings.Add(L"Espa\u00F1ol");             // Spanish
  m_interface_lang_strings.Add(L"Fran\u00E7ais");            // French
  m_interface_lang_strings.Add(L"Hrvatski");                 // Croatian
  m_interface_lang_strings.Add(L"Italiano");                 // Italian
  m_interface_lang_strings.Add(L"Magyar");                   // Hungarian
  m_interface_lang_strings.Add(L"Nederlands");               // Dutch
  m_interface_lang_strings.Add(L"Norsk bokm\u00E5l");        // Norwegian
  m_interface_lang_strings.Add(L"Polski");                   // Polish
  m_interface_lang_strings.Add(L"Portugu\u00EAs");           // Portuguese
  m_interface_lang_strings.Add(L"Portugu\u00EAs (Brasil)");  // Portuguese (Brazil)
  m_interface_lang_strings.Add(L"Rom\u00E2n\u0103");         // Romanian
  m_interface_lang_strings.Add(L"Srpski");                   // Serbian
  m_interface_lang_strings.Add(L"Svenska");                  // Swedish
  m_interface_lang_strings.Add(L"T\u00FCrk\u00E7e");         // Turkish

  m_interface_lang_strings.Add(L"\u0395\u03BB\u03BB\u03B7\u03BD\u03B9\u03BA\u03AC");  // Greek
  m_interface_lang_strings.Add(L"\u0420\u0443\u0441\u0441\u043A\u0438\u0439");        // Russian
  m_interface_lang_strings.Add(L"\u0627\u0644\u0639\u0631\u0628\u064A\u0629");        // Arabic
  m_interface_lang_strings.Add(L"\u0641\u0627\u0631\u0633\u06CC");                    // Farsi
  m_interface_lang_strings.Add(L"\uD55C\uAD6D\uC5B4");                                // Korean
  m_interface_lang_strings.Add(L"\u65E5\u672C\u8A9E");                                // Japanese
  m_interface_lang_strings.Add(L"\u7B80\u4F53\u4E2D\u6587");  // Simplified Chinese
  m_interface_lang_strings.Add(L"\u7E41\u9AD4\u4E2D\u6587");  // Traditional Chinese

  m_confirm_stop_checkbox = new wxCheckBox(this, wxID_ANY, _("Confirm on Stop"));
  m_panic_handlers_checkbox = new wxCheckBox(this, wxID_ANY, _("Use Panic Handlers"));
  m_osd_messages_checkbox = new wxCheckBox(this, wxID_ANY, _("Show On-Screen Messages"));
  m_show_active_title_checkbox =
      new wxCheckBox(this, wxID_ANY, _("Show Active Title in Window Title"));
  m_use_builtin_title_database_checkbox =
      new wxCheckBox(this, wxID_ANY, _("Use Built-In Database of Game Names"));
  m_pause_focus_lost_checkbox = new wxCheckBox(this, wxID_ANY, _("Pause on Focus Loss"));
  m_interface_lang_choice =
      new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_interface_lang_strings);
  m_theme_choice = new wxChoice(this, wxID_ANY);

  m_confirm_stop_checkbox->Bind(wxEVT_CHECKBOX, &InterfaceConfigPane::OnConfirmStopCheckBoxChanged,
                                this);
  m_panic_handlers_checkbox->Bind(wxEVT_CHECKBOX,
                                  &InterfaceConfigPane::OnPanicHandlersCheckBoxChanged, this);
  m_osd_messages_checkbox->Bind(wxEVT_CHECKBOX, &InterfaceConfigPane::OnOSDMessagesCheckBoxChanged,
                                this);
  m_show_active_title_checkbox->Bind(wxEVT_CHECKBOX,
                                     &InterfaceConfigPane::OnShowActiveTitleCheckBoxChanged, this);
  m_use_builtin_title_database_checkbox->Bind(
      wxEVT_CHECKBOX, &InterfaceConfigPane::OnUseBuiltinTitleDatabaseCheckBoxChanged, this);
  m_pause_focus_lost_checkbox->Bind(wxEVT_CHECKBOX,
                                    &InterfaceConfigPane::OnPauseOnFocusLostCheckBoxChanged, this);
  m_interface_lang_choice->Bind(wxEVT_CHOICE,
                                &InterfaceConfigPane::OnInterfaceLanguageChoiceChanged, this);
  m_theme_choice->Bind(wxEVT_CHOICE, &InterfaceConfigPane::OnThemeSelected, this);

  m_confirm_stop_checkbox->SetToolTip(_("Show a confirmation box before stopping a game."));
  m_panic_handlers_checkbox->SetToolTip(
      _("Show a message box when a potentially serious error has occurred.\nDisabling this may "
        "avoid annoying and non-fatal messages, but it may result in major crashes having no "
        "explanation at all."));
  m_osd_messages_checkbox->SetToolTip(
      _("Display messages over the emulation screen area.\nThese messages include memory card "
        "writes, video backend and CPU information, and JIT cache clearing."));
  m_show_active_title_checkbox->SetToolTip(
      _("Show the active title name in the emulation window title."));
  m_use_builtin_title_database_checkbox->SetToolTip(
      _("Read game names from an internal database instead of reading names from the games "
        "themselves, except for games that aren't in the database. The names in the database are "
        "often more consistently formatted, especially for Wii games."));
  m_pause_focus_lost_checkbox->SetToolTip(
      _("Pauses the emulator when focus is taken away from the emulation window."));
  m_interface_lang_choice->SetToolTip(
      _("Change the language of the user interface.\nRequires restart."));

  const int space5 = FromDIP(5);

  wxGridBagSizer* const language_and_theme_grid_sizer = new wxGridBagSizer(space5, space5);
  language_and_theme_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("Language:")),
                                     wxGBPosition(0, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  language_and_theme_grid_sizer->Add(m_interface_lang_choice, wxGBPosition(0, 1), wxDefaultSpan,
                                     wxALIGN_CENTER_VERTICAL);
  language_and_theme_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("Theme:")),
                                     wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
  language_and_theme_grid_sizer->Add(m_theme_choice, wxGBPosition(1, 1), wxDefaultSpan,
                                     wxALIGN_CENTER_VERTICAL);

  wxStaticBoxSizer* const main_static_box_sizer =
      new wxStaticBoxSizer(wxVERTICAL, this, _("Interface Settings"));
  main_static_box_sizer->AddSpacer(space5);
  main_static_box_sizer->Add(m_confirm_stop_checkbox, 0, wxLEFT | wxRIGHT, space5);
  main_static_box_sizer->AddSpacer(space5);
  main_static_box_sizer->Add(m_panic_handlers_checkbox, 0, wxLEFT | wxRIGHT, space5);
  main_static_box_sizer->AddSpacer(space5);
  main_static_box_sizer->Add(m_osd_messages_checkbox, 0, wxLEFT | wxRIGHT, space5);
  main_static_box_sizer->AddSpacer(space5);
  main_static_box_sizer->Add(m_show_active_title_checkbox, 0, wxLEFT | wxRIGHT, space5);
  main_static_box_sizer->AddSpacer(space5);
  main_static_box_sizer->Add(m_use_builtin_title_database_checkbox, 0, wxLEFT | wxRIGHT, space5);
  main_static_box_sizer->AddSpacer(space5);
  main_static_box_sizer->Add(m_pause_focus_lost_checkbox, 0, wxLEFT | wxRIGHT, space5);
  main_static_box_sizer->AddSpacer(space5);
  main_static_box_sizer->Add(language_and_theme_grid_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_static_box_sizer->AddSpacer(space5);

  wxBoxSizer* const main_box_sizer = new wxBoxSizer(wxVERTICAL);
  main_box_sizer->AddSpacer(space5);
  main_box_sizer->Add(main_static_box_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT, space5);
  main_box_sizer->AddSpacer(space5);

  SetSizer(main_box_sizer);
}

void InterfaceConfigPane::LoadGUIValues()
{
  const SConfig& startup_params = SConfig::GetInstance();

  m_confirm_stop_checkbox->SetValue(startup_params.bConfirmStop);
  m_panic_handlers_checkbox->SetValue(startup_params.bUsePanicHandlers);
  m_osd_messages_checkbox->SetValue(startup_params.bOnScreenDisplayMessages);
  m_show_active_title_checkbox->SetValue(startup_params.m_show_active_title);
  m_use_builtin_title_database_checkbox->SetValue(startup_params.m_use_builtin_title_database);
  m_pause_focus_lost_checkbox->SetValue(SConfig::GetInstance().m_PauseOnFocusLost);

  const std::string exact_language = SConfig::GetInstance().m_InterfaceLanguage;
  const std::string loose_language = exact_language.substr(0, exact_language.find('_'));
  size_t exact_match_index = std::numeric_limits<size_t>::max();
  size_t loose_match_index = std::numeric_limits<size_t>::max();
  for (size_t i = 0; i < language_ids.size(); i++)
  {
    if (language_ids[i] == exact_language)
    {
      exact_match_index = i;
      break;
    }
    else if (language_ids[i] == loose_language)
    {
      loose_match_index = i;
    }
  }
  if (exact_match_index != std::numeric_limits<size_t>::max())
    m_interface_lang_choice->SetSelection(exact_match_index);
  else if (loose_match_index != std::numeric_limits<size_t>::max())
    m_interface_lang_choice->SetSelection(loose_match_index);

  LoadThemes();
}

void InterfaceConfigPane::LoadThemes()
{
  auto sv =
      Common::DoFileSearch({File::GetUserPath(D_THEMES_IDX), File::GetSysDirectory() + THEMES_DIR});
  for (const std::string& filename : sv)
  {
    std::string name, ext;
    SplitPath(filename, nullptr, &name, &ext);

    name += ext;
    const wxString wxname = StrToWxStr(name);
    if (-1 == m_theme_choice->FindString(wxname))
      m_theme_choice->Append(wxname);
  }

  m_theme_choice->SetStringSelection(StrToWxStr(SConfig::GetInstance().theme_name));
}

void InterfaceConfigPane::OnConfirmStopCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().bConfirmStop = m_confirm_stop_checkbox->IsChecked();
}

void InterfaceConfigPane::OnPanicHandlersCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().bUsePanicHandlers = m_panic_handlers_checkbox->IsChecked();
  SetEnableAlert(m_panic_handlers_checkbox->IsChecked());
}

void InterfaceConfigPane::OnOSDMessagesCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().bOnScreenDisplayMessages = m_osd_messages_checkbox->IsChecked();
}

void InterfaceConfigPane::OnShowActiveTitleCheckBoxChanged(wxCommandEvent&)
{
  SConfig::GetInstance().m_show_active_title = m_show_active_title_checkbox->IsChecked();
}

void InterfaceConfigPane::OnUseBuiltinTitleDatabaseCheckBoxChanged(wxCommandEvent&)
{
  SConfig::GetInstance().m_use_builtin_title_database =
      m_use_builtin_title_database_checkbox->IsChecked();
}

void InterfaceConfigPane::OnInterfaceLanguageChoiceChanged(wxCommandEvent& event)
{
  if (SConfig::GetInstance().m_InterfaceLanguage !=
      language_ids[m_interface_lang_choice->GetSelection()])
  {
    wxMessageBox(_("You must restart Dolphin in order for the change to take effect."),
                 _("Restart Required"), wxOK | wxICON_INFORMATION, this);
  }

  SConfig::GetInstance().m_InterfaceLanguage =
      language_ids[m_interface_lang_choice->GetSelection()];
}

void InterfaceConfigPane::OnPauseOnFocusLostCheckBoxChanged(wxCommandEvent& event)
{
  SConfig::GetInstance().m_PauseOnFocusLost = m_pause_focus_lost_checkbox->IsChecked();
}

void InterfaceConfigPane::OnThemeSelected(wxCommandEvent& event)
{
  SConfig::GetInstance().theme_name = WxStrToStr(m_theme_choice->GetStringSelection());

  wxCommandEvent theme_event{DOLPHIN_EVT_RELOAD_THEME_BITMAPS};
  theme_event.SetEventObject(this);
  ProcessEvent(theme_event);
}
