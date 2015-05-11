// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/gbsizer.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "DiscIO/Volume.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Config/WiiConfigPane.h"

WiiConfigPane::WiiConfigPane(wxWindow* parent, wxWindowID id)
	: wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	RefreshGUI();
}

void WiiConfigPane::InitializeGUI()
{
	m_aspect_ratio_strings.Add("4:3");
	m_aspect_ratio_strings.Add("16:9");

	m_system_language_strings.Add(_("Japanese"));
	m_system_language_strings.Add(_("English"));
	m_system_language_strings.Add(_("German"));
	m_system_language_strings.Add(_("French"));
	m_system_language_strings.Add(_("Spanish"));
	m_system_language_strings.Add(_("Italian"));
	m_system_language_strings.Add(_("Dutch"));
	m_system_language_strings.Add(_("Simplified Chinese"));
	m_system_language_strings.Add(_("Traditional Chinese"));
	m_system_language_strings.Add(_("Korean"));

	m_screensaver_checkbox = new wxCheckBox(this, wxID_ANY, _("Enable Screen Saver"));
	m_pal60_mode_checkbox = new wxCheckBox(this, wxID_ANY, _("Use PAL60 Mode (EuRGB60)"));
	m_aspect_ratio_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_aspect_ratio_strings);
	m_system_language_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_system_language_strings);
	m_sd_card_checkbox = new wxCheckBox(this, wxID_ANY, _("Insert SD Card"));
	m_connect_keyboard_checkbox = new wxCheckBox(this, wxID_ANY, _("Connect USB Keyboard"));

	m_screensaver_checkbox->Bind(wxEVT_CHECKBOX, &WiiConfigPane::OnScreenSaverCheckBoxChanged, this);
	m_pal60_mode_checkbox->Bind(wxEVT_CHECKBOX, &WiiConfigPane::OnPAL60CheckBoxChanged, this);
	m_aspect_ratio_choice->Bind(wxEVT_CHOICE, &WiiConfigPane::OnAspectRatioChoiceChanged, this);
	m_system_language_choice->Bind(wxEVT_CHOICE, &WiiConfigPane::OnSystemLanguageChoiceChanged, this);
	m_sd_card_checkbox->Bind(wxEVT_CHECKBOX, &WiiConfigPane::OnSDCardCheckBoxChanged, this);
	m_connect_keyboard_checkbox->Bind(wxEVT_CHECKBOX, &WiiConfigPane::OnConnectKeyboardCheckBoxChanged, this);

	m_screensaver_checkbox->SetToolTip(_("Dims the screen after five minutes of inactivity."));
	m_pal60_mode_checkbox->SetToolTip(_("Sets the Wii display mode to 60Hz (480i) instead of 50Hz (576i) for PAL games.\nMay not work for all games."));
	m_system_language_choice->SetToolTip(_("Sets the Wii system language."));
	m_sd_card_checkbox->SetToolTip(_("Saved to /Wii/sd.raw (default size is 128mb)"));
	m_connect_keyboard_checkbox->SetToolTip(_("May cause slow down in Wii Menu and some games."));

	wxGridBagSizer* const misc_settings_grid_sizer = new wxGridBagSizer();
	misc_settings_grid_sizer->Add(m_screensaver_checkbox, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	misc_settings_grid_sizer->Add(m_pal60_mode_checkbox, wxGBPosition(1, 0), wxGBSpan(1, 2), wxALL, 5);
	misc_settings_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("Aspect Ratio:")), wxGBPosition(2, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	misc_settings_grid_sizer->Add(m_aspect_ratio_choice, wxGBPosition(2, 1), wxDefaultSpan, wxALL, 5);
	misc_settings_grid_sizer->Add(new wxStaticText(this, wxID_ANY, _("System Language:")), wxGBPosition(3, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxALL, 5);
	misc_settings_grid_sizer->Add(m_system_language_choice, wxGBPosition(3, 1), wxDefaultSpan, wxALL, 5);

	wxStaticBoxSizer* const misc_settings_static_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Misc Settings"));
	misc_settings_static_sizer->Add(misc_settings_grid_sizer);

	wxStaticBoxSizer* const device_settings_sizer = new wxStaticBoxSizer(wxVERTICAL, this, _("Device Settings"));
	device_settings_sizer->Add(m_sd_card_checkbox, 0, wxALL, 5);
	device_settings_sizer->Add(m_connect_keyboard_checkbox, 0, wxALL, 5);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(misc_settings_static_sizer, 0, wxEXPAND | wxALL, 5);
	main_sizer->Add(device_settings_sizer, 0, wxEXPAND | wxALL, 5);

	SetSizer(main_sizer);
}

void WiiConfigPane::LoadGUIValues()
{
	m_screensaver_checkbox->SetValue(!!SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.SSV"));
	m_pal60_mode_checkbox->SetValue(!!SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.E60"));
	m_aspect_ratio_choice->SetSelection(SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.AR"));
	m_system_language_choice->SetSelection(SConfig::GetInstance().m_SYSCONF->GetData<u8>("IPL.LNG"));

	m_sd_card_checkbox->SetValue(SConfig::GetInstance().m_WiiSDCard);
	m_connect_keyboard_checkbox->SetValue(SConfig::GetInstance().m_WiiKeyboard);
}

void WiiConfigPane::RefreshGUI()
{
	if (Core::IsRunning())
	{
		m_screensaver_checkbox->Disable();
		m_pal60_mode_checkbox->Disable();
		m_aspect_ratio_choice->Disable();
		m_system_language_choice->Disable();
	}
}

void WiiConfigPane::OnScreenSaverCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_SYSCONF->SetData("IPL.SSV", m_screensaver_checkbox->IsChecked());
}

void WiiConfigPane::OnPAL60CheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_SYSCONF->SetData("IPL.E60", m_pal60_mode_checkbox->IsChecked());
}

void WiiConfigPane::OnSDCardCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_WiiSDCard = m_sd_card_checkbox->IsChecked();
	WII_IPC_HLE_Interface::SDIO_EventNotify();
}

void WiiConfigPane::OnConnectKeyboardCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_WiiKeyboard = m_connect_keyboard_checkbox->IsChecked();
}

void WiiConfigPane::OnSystemLanguageChoiceChanged(wxCommandEvent& event)
{
	DiscIO::IVolume::ELanguage wii_system_lang = (DiscIO::IVolume::ELanguage)m_system_language_choice->GetSelection();
	SConfig::GetInstance().m_SYSCONF->SetData("IPL.LNG", wii_system_lang);
	u8 country_code = GetSADRCountryCode(wii_system_lang);

	if (!SConfig::GetInstance().m_SYSCONF->SetArrayData("IPL.SADR", &country_code, 1))
		WxUtils::ShowErrorDialog(_("Failed to update country code in SYSCONF"));
}

void WiiConfigPane::OnAspectRatioChoiceChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().m_SYSCONF->SetData("IPL.AR", m_aspect_ratio_choice->GetSelection());
}

// Change from IPL.LNG value to IPL.SADR country code.
// http://wiibrew.org/wiki/Country_Codes
u8 WiiConfigPane::GetSADRCountryCode(DiscIO::IVolume::ELanguage language)
{
	switch (language)
	{
	case DiscIO::IVolume::LANGUAGE_JAPANESE:
		return 1;   // Japan
	case DiscIO::IVolume::LANGUAGE_ENGLISH:
		return 49;  // USA
	case DiscIO::IVolume::LANGUAGE_GERMAN:
		return 78;  // Germany
	case DiscIO::IVolume::LANGUAGE_FRENCH:
		return 77;  // France
	case DiscIO::IVolume::LANGUAGE_SPANISH:
		return 105; // Spain
	case DiscIO::IVolume::LANGUAGE_ITALIAN:
		return 83;  // Italy
	case DiscIO::IVolume::LANGUAGE_DUTCH:
		return 94;  // Netherlands
	case DiscIO::IVolume::LANGUAGE_SIMPLIFIED_CHINESE:
	case DiscIO::IVolume::LANGUAGE_TRADITIONAL_CHINESE:
		return 157; // China
	case DiscIO::IVolume::LANGUAGE_KOREAN:
		return 136; // Korea
	case DiscIO::IVolume::LANGUAGE_UNKNOWN:
		break;
	}

	PanicAlert("Invalid language. Defaulting to Japanese.");
	return 1;
}
