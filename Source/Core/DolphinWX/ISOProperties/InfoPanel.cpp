// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinWX/ISOProperties/InfoPanel.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <wx/arrstr.h>
#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/gbsizer.h>
#include <wx/menu.h>
#include <wx/progdlg.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

#include "Common/MD5.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/IOS/ES/Formats.h"
#include "DiscIO/Enums.h"
#include "DiscIO/Volume.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/ISOProperties/ISOProperties.h"
#include "DolphinWX/WxUtils.h"

namespace
{
template <typename T>
wxString OptionalToString(std::optional<T> value)
{
  return value ? StrToWxStr(std::to_string(*value)) : wxString();
}

wxArrayString GetLanguageChoiceStrings(const std::vector<DiscIO::Language>& languages)
{
  wxArrayString available_languages;

  for (auto language : languages)
  {
    switch (language)
    {
    case DiscIO::Language::LANGUAGE_JAPANESE:
      available_languages.Add(_("Japanese"));
      break;
    case DiscIO::Language::LANGUAGE_ENGLISH:
      available_languages.Add(_("English"));
      break;
    case DiscIO::Language::LANGUAGE_GERMAN:
      available_languages.Add(_("German"));
      break;
    case DiscIO::Language::LANGUAGE_FRENCH:
      available_languages.Add(_("French"));
      break;
    case DiscIO::Language::LANGUAGE_SPANISH:
      available_languages.Add(_("Spanish"));
      break;
    case DiscIO::Language::LANGUAGE_ITALIAN:
      available_languages.Add(_("Italian"));
      break;
    case DiscIO::Language::LANGUAGE_DUTCH:
      available_languages.Add(_("Dutch"));
      break;
    case DiscIO::Language::LANGUAGE_SIMPLIFIED_CHINESE:
      available_languages.Add(_("Simplified Chinese"));
      break;
    case DiscIO::Language::LANGUAGE_TRADITIONAL_CHINESE:
      available_languages.Add(_("Traditional Chinese"));
      break;
    case DiscIO::Language::LANGUAGE_KOREAN:
      available_languages.Add(_("Korean"));
      break;
    case DiscIO::Language::LANGUAGE_UNKNOWN:
    default:
      available_languages.Add(_("Unknown"));
      break;
    }
  }

  return available_languages;
}

wxString GetCountryName(DiscIO::Country country)
{
  switch (country)
  {
  case DiscIO::Country::COUNTRY_AUSTRALIA:
    return _("Australia");
  case DiscIO::Country::COUNTRY_EUROPE:
    return _("Europe");
  case DiscIO::Country::COUNTRY_FRANCE:
    return _("France");
  case DiscIO::Country::COUNTRY_ITALY:
    return _("Italy");
  case DiscIO::Country::COUNTRY_GERMANY:
    return _("Germany");
  case DiscIO::Country::COUNTRY_NETHERLANDS:
    return _("Netherlands");
  case DiscIO::Country::COUNTRY_RUSSIA:
    return _("Russia");
  case DiscIO::Country::COUNTRY_SPAIN:
    return _("Spain");
  case DiscIO::Country::COUNTRY_USA:
    return _("USA");
  case DiscIO::Country::COUNTRY_JAPAN:
    return _("Japan");
  case DiscIO::Country::COUNTRY_KOREA:
    return _("Korea");
  case DiscIO::Country::COUNTRY_TAIWAN:
    return _("Taiwan");
  case DiscIO::Country::COUNTRY_WORLD:
    return _("World");
  case DiscIO::Country::COUNTRY_UNKNOWN:
  default:
    return _("Unknown");
  }
}

int FindPreferredLanguageIndex(DiscIO::Language preferred_language,
                               const std::vector<DiscIO::Language>& languages)
{
  const auto iter =
      std::find_if(languages.begin(), languages.end(),
                   [preferred_language](auto language) { return language == preferred_language; });

  if (iter == languages.end())
    return 0;

  return static_cast<int>(std::distance(languages.begin(), iter));
}
}  // Anonymous namespace

InfoPanel::InfoPanel(wxWindow* parent, wxWindowID id, const GameListItem& item,
                     const std::unique_ptr<DiscIO::Volume>& opened_iso)
    : wxPanel{parent, id}, m_game_list_item{item}, m_opened_iso{opened_iso}
{
  CreateGUI();
  BindEvents();
  LoadGUIData();
}

void InfoPanel::CreateGUI()
{
  const int space_5 = FromDIP(5);

  auto* const main_sizer = new wxBoxSizer(wxVERTICAL);
  main_sizer->AddSpacer(space_5);
  main_sizer->Add(CreateISODetailsSizer(), 0, wxEXPAND | wxLEFT | wxRIGHT, space_5);
  main_sizer->AddSpacer(space_5);
  main_sizer->Add(CreateBannerDetailsSizer(), 0, wxEXPAND | wxLEFT | wxRIGHT, space_5);
  main_sizer->AddSpacer(space_5);

  SetSizer(main_sizer);
}

void InfoPanel::BindEvents()
{
  m_md5_sum_compute->Bind(wxEVT_BUTTON, &InfoPanel::OnComputeMD5, this);
  m_languages->Bind(wxEVT_CHOICE, &InfoPanel::OnChangeBannerLanguage, this);

  Bind(wxEVT_MENU, &InfoPanel::OnSaveBannerImage, this, IDM_SAVE_BANNER);
}

void InfoPanel::LoadGUIData()
{
  LoadISODetails();
  LoadBannerDetails();
}

void InfoPanel::LoadISODetails()
{
  m_internal_name->SetValue(StrToWxStr(m_opened_iso->GetInternalName()));
  m_game_id->SetValue(StrToWxStr(m_opened_iso->GetGameID()));
  m_country->SetValue(GetCountryName(m_opened_iso->GetCountry()));
  m_maker_id->SetValue("0x" + StrToWxStr(m_opened_iso->GetMakerID()));
  m_revision->SetValue(OptionalToString(m_opened_iso->GetRevision()));
  m_date->SetValue(StrToWxStr(m_opened_iso->GetApploaderDate()));
  if (m_ios_version)
  {
    const IOS::ES::TMDReader tmd = m_opened_iso->GetTMD(m_opened_iso->GetGamePartition());
    if (tmd.IsValid())
      m_ios_version->SetValue(StringFromFormat("IOS%u", static_cast<u32>(tmd.GetIOSId())));
  }
}

void InfoPanel::LoadBannerDetails()
{
  LoadBannerImage();

  const bool is_wii = DiscIO::IsWii(m_opened_iso->GetVolumeType());
  ChangeBannerDetails(SConfig::GetInstance().GetCurrentLanguage(is_wii));
}

void InfoPanel::LoadBannerImage()
{
  const auto& banner_image = m_game_list_item.GetBannerImage();
  const auto banner_min_size = m_banner->GetMinSize();

  if (banner_image.IsOk())
  {
    m_banner->SetBitmap(WxUtils::ScaleImageToBitmap(banner_image, this, banner_min_size));
    m_banner->Bind(wxEVT_RIGHT_DOWN, &InfoPanel::OnRightClickBanner, this);
  }
  else
  {
    m_banner->SetBitmap(WxUtils::LoadScaledResourceBitmap("nobanner", this, banner_min_size));
  }
}

wxStaticBoxSizer* InfoPanel::CreateISODetailsSizer()
{
  std::vector<std::pair<wxString, wxTextCtrl*&>> controls = {{
      {_("Internal Name:"), m_internal_name},
      {_("Game ID:"), m_game_id},
      {_("Country:"), m_country},
      {_("Maker ID:"), m_maker_id},
      {_("Revision:"), m_revision},
      {_("Apploader Date:"), m_date},
  }};
  if (m_opened_iso->GetTMD(m_opened_iso->GetGamePartition()).IsValid())
    controls.emplace_back(_("IOS Version:"), m_ios_version);

  const int space_10 = FromDIP(10);
  auto* const iso_details = new wxGridBagSizer(space_10, space_10);
  size_t row = 0;
  for (auto& control : controls)
  {
    auto* const text = new wxStaticText(this, wxID_ANY, control.first);
    iso_details->Add(text, wxGBPosition(row, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL);
    control.second = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                    wxTE_READONLY);
    iso_details->Add(control.second, wxGBPosition(row, 1), wxGBSpan(1, 2), wxEXPAND);
    ++row;
  }

  auto* const md5_sum_text = new wxStaticText(this, wxID_ANY, _("MD5 Checksum:"));
  m_md5_sum = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                             wxTE_READONLY);
  m_md5_sum_compute = new wxButton(this, wxID_ANY, _("Compute"));

  iso_details->Add(md5_sum_text, wxGBPosition(row, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL);
  iso_details->Add(m_md5_sum, wxGBPosition(row, 1), wxGBSpan(1, 1), wxEXPAND);
  iso_details->Add(m_md5_sum_compute, wxGBPosition(row, 2), wxGBSpan(1, 1), wxEXPAND);
  ++row;

  iso_details->AddGrowableCol(1);

  const int space_5 = FromDIP(5);
  auto* const iso_details_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("ISO Details"));
  iso_details_sizer->AddSpacer(space_5);
  iso_details_sizer->Add(iso_details, 0, wxEXPAND | wxLEFT | wxRIGHT, space_5);
  iso_details_sizer->AddSpacer(space_5);

  return iso_details_sizer;
}

wxStaticBoxSizer* InfoPanel::CreateBannerDetailsSizer()
{
  auto* const name_text = new wxStaticText(this, wxID_ANY, _("Name:"));
  m_name = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                          wxTE_READONLY);
  auto* const maker_text = new wxStaticText(this, wxID_ANY, _("Maker:"));
  m_maker = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                           wxTE_READONLY);
  auto* const comment_text = new wxStaticText(this, wxID_ANY, _("Description:"));
  m_comment = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                             wxTE_MULTILINE | wxTE_READONLY);
  auto* const banner_text = new wxStaticText(this, wxID_ANY, _("Banner:"));
  m_banner =
      new wxStaticBitmap(this, wxID_ANY, wxNullBitmap, wxDefaultPosition, FromDIP(wxSize(96, 32)));

  auto* const languages_text = new wxStaticText(this, wxID_ANY, _("Show Language:"));
  m_languages = CreateCommentLanguageChoice();

  const int space_10 = FromDIP(10);
  auto* const banner_details = new wxGridBagSizer(space_10, space_10);
  banner_details->Add(languages_text, wxGBPosition(0, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL);
  // Comboboxes cannot be safely stretched vertically on Windows.
  banner_details->Add(WxUtils::GiveMinSize(m_languages, wxDefaultSize), wxGBPosition(0, 1),
                      wxGBSpan(1, 1), wxEXPAND);
  banner_details->Add(name_text, wxGBPosition(1, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL);
  banner_details->Add(m_name, wxGBPosition(1, 1), wxGBSpan(1, 1), wxEXPAND);
  banner_details->Add(maker_text, wxGBPosition(2, 0), wxGBSpan(1, 1), wxALIGN_CENTER_VERTICAL);
  banner_details->Add(m_maker, wxGBPosition(2, 1), wxGBSpan(1, 1), wxEXPAND);
  banner_details->Add(comment_text, wxGBPosition(3, 0), wxGBSpan(1, 1));
  banner_details->Add(m_comment, wxGBPosition(3, 1), wxGBSpan(1, 1), wxEXPAND);
  banner_details->Add(banner_text, wxGBPosition(4, 0), wxGBSpan(1, 1));
  banner_details->Add(m_banner, wxGBPosition(4, 1), wxGBSpan(1, 1));
  banner_details->AddGrowableCol(1);

  const int space_5 = FromDIP(5);
  auto* const banner_details_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Banner Details"));
  banner_details_sizer->AddSpacer(space_5);
  banner_details_sizer->Add(banner_details, 0, wxEXPAND | wxLEFT | wxRIGHT, space_5);
  banner_details_sizer->AddSpacer(space_5);

  return banner_details_sizer;
}

wxChoice* InfoPanel::CreateCommentLanguageChoice()
{
  const auto languages = m_game_list_item.GetLanguages();
  const bool is_wii = DiscIO::IsWii(m_opened_iso->GetVolumeType());
  const auto preferred_language = SConfig::GetInstance().GetCurrentLanguage(is_wii);
  const int preferred_language_index = FindPreferredLanguageIndex(preferred_language, languages);
  const auto choices = GetLanguageChoiceStrings(languages);

  auto* const choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, choices);
  choice->SetSelection(preferred_language_index);

  if (choice->GetCount() <= 1)
    choice->Disable();

  return choice;
}

void InfoPanel::OnComputeMD5(wxCommandEvent& WXUNUSED(event))
{
  wxProgressDialog progress_dialog(_("Computing MD5 checksum"), _("Working..."), 100, this,
                                   wxPD_APP_MODAL | wxPD_AUTO_HIDE | wxPD_CAN_ABORT |
                                       wxPD_ELAPSED_TIME | wxPD_ESTIMATED_TIME |
                                       wxPD_REMAINING_TIME | wxPD_SMOOTH);

  const auto result = MD5::MD5Sum(m_game_list_item.GetFileName(), [&progress_dialog](int progress) {
    return progress_dialog.Update(progress);
  });

  if (progress_dialog.WasCancelled())
    return;

  m_md5_sum->SetValue(result);
}

void InfoPanel::OnChangeBannerLanguage(wxCommandEvent& event)
{
  ChangeBannerDetails(m_game_list_item.GetLanguages()[event.GetSelection()]);
}

void InfoPanel::OnRightClickBanner(wxMouseEvent& WXUNUSED(event))
{
  wxMenu menu;
  menu.Append(IDM_SAVE_BANNER, _("Save as..."));
  PopupMenu(&menu);
}

void InfoPanel::OnSaveBannerImage(wxCommandEvent& WXUNUSED(event))
{
  wxFileDialog dialog(this, _("Save as..."), wxGetHomeDir(),
                      wxString::Format("%s.png", m_game_id->GetValue()), wxALL_FILES_PATTERN,
                      wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

  if (dialog.ShowModal() == wxID_OK)
  {
    m_game_list_item.GetBannerImage().SaveFile(dialog.GetPath());
  }

  Raise();
}

void InfoPanel::ChangeBannerDetails(DiscIO::Language language)
{
  const auto name = StrToWxStr(m_game_list_item.GetName(language));
  const auto comment = StrToWxStr(m_game_list_item.GetDescription(language));
  const auto maker = StrToWxStr(m_game_list_item.GetCompany());

  m_name->SetValue(name);
  m_comment->SetValue(comment);
  m_maker->SetValue(maker);

  std::string path, filename, extension;
  SplitPath(m_game_list_item.GetFileName(), &path, &filename, &extension);

  // Real disk drives don't have filenames on Windows
  if (filename.empty() && extension.empty())
    filename = path + ' ';

  const auto game_id = m_game_list_item.GetGameID();
  const auto new_title = wxString::Format("%s%s: %s - %s", StrToWxStr(filename),
                                          StrToWxStr(extension), StrToWxStr(game_id), name);

  EmitTitleChangeEvent(new_title);
}

void InfoPanel::EmitTitleChangeEvent(const wxString& new_title)
{
  wxCommandEvent event{DOLPHIN_EVT_CHANGE_ISO_PROPERTIES_TITLE, GetId()};
  event.SetEventObject(this);
  event.SetString(new_title);
  AddPendingEvent(event);
}
