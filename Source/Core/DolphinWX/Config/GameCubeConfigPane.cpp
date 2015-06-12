// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/gbsizer.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/NetPlayProto.h"
#include "Core/HW/EXI.h"
#include "Core/HW/GCMemcard.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/Config/ConfigMain.h"
#include "DolphinWX/Config/GameCubeConfigPane.h"

#define DEV_NONE_STR        _trans("<Nothing>")
#define DEV_DUMMY_STR       _trans("Dummy")

#define EXIDEV_MEMCARD_STR  _trans("Memory Card")
#define EXIDEV_MEMDIR_STR   _trans("GCI Folder")
#define EXIDEV_MIC_STR      _trans("Mic")
#define EXIDEV_BBA_STR      "BBA"
#define EXIDEV_AGP_STR      "Advance Game Port"
#define EXIDEV_AM_BB_STR    _trans("AM-Baseboard")
#define EXIDEV_GECKO_STR    "USBGecko"

GameCubeConfigPane::GameCubeConfigPane(wxWindow* parent, wxWindowID id)
	: wxPanel(parent, id)
{
	InitializeGUI();
	LoadGUIValues();
	RefreshGUI();
}

void GameCubeConfigPane::InitializeGUI()
{
	m_ipl_language_strings.Add(_("English"));
	m_ipl_language_strings.Add(_("German"));
	m_ipl_language_strings.Add(_("French"));
	m_ipl_language_strings.Add(_("Spanish"));
	m_ipl_language_strings.Add(_("Italian"));
	m_ipl_language_strings.Add(_("Dutch"));

	m_system_lang_choice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_ipl_language_strings);
	m_system_lang_choice->SetToolTip(_("Sets the GameCube system language."));
	m_system_lang_choice->Bind(wxEVT_CHOICE, &GameCubeConfigPane::OnSystemLanguageChange, this);

	m_override_lang_checkbox = new wxCheckBox(this, wxID_ANY, _("Override Language on NTSC Games"));
	m_override_lang_checkbox->SetToolTip(_("Lets the system language be set to values that games were not designed for. This can allow the use of extra translations for a few games, but can also lead to text display issues."));
	m_override_lang_checkbox->Bind(wxEVT_CHECKBOX, &GameCubeConfigPane::OnOverrideLanguageCheckBoxChanged, this);

	m_skip_bios_checkbox = new wxCheckBox(this, wxID_ANY, _("Skip BIOS"));
	m_skip_bios_checkbox->Bind(wxEVT_CHECKBOX, &GameCubeConfigPane::OnSkipBiosCheckBoxChanged, this);

	if (!File::Exists(File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + USA_DIR + DIR_SEP GC_IPL) &&
	    !File::Exists(File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + USA_DIR + DIR_SEP GC_IPL) &&
	    !File::Exists(File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + JAP_DIR + DIR_SEP GC_IPL) &&
	    !File::Exists(File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + JAP_DIR + DIR_SEP GC_IPL) &&
	    !File::Exists(File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + EUR_DIR + DIR_SEP GC_IPL) &&
	    !File::Exists(File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + EUR_DIR + DIR_SEP GC_IPL))
	{
		m_skip_bios_checkbox->Disable();
		m_skip_bios_checkbox->SetToolTip(_("Put BIOS roms in User/GC/{region}."));
	}

	// Device settings
	// EXI Devices
	wxStaticText* GCEXIDeviceText[3] = {
		new wxStaticText(this, wxID_ANY, _("Slot A")),
		new wxStaticText(this, wxID_ANY, _("Slot B")),
		new wxStaticText(this, wxID_ANY, "SP1"),
	};

	m_exi_devices[0] = new wxChoice(this, wxID_ANY);
	m_exi_devices[0]->Bind(wxEVT_CHOICE, &GameCubeConfigPane::OnSlotAChanged, this);
	m_exi_devices[1] = new wxChoice(this, wxID_ANY);
	m_exi_devices[1]->Bind(wxEVT_CHOICE, &GameCubeConfigPane::OnSlotBChanged, this);
	m_exi_devices[2] = new wxChoice(this, wxID_ANY);
	m_exi_devices[2]->Bind(wxEVT_CHOICE, &GameCubeConfigPane::OnSP1Changed, this);
	m_exi_devices[2]->SetToolTip(_("Serial Port 1 - This is the port which devices such as the net adapter use."));

	m_memcard_path[0] = new wxButton(this, wxID_ANY, "...", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	m_memcard_path[0]->Bind(wxEVT_BUTTON, &GameCubeConfigPane::OnSlotAButtonClick, this);
	m_memcard_path[1] = new wxButton(this, wxID_ANY, "...", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
	m_memcard_path[1]->Bind(wxEVT_BUTTON, &GameCubeConfigPane::OnSlotBButtonClick, this);

	// Populate the GameCube page
	wxGridBagSizer* const sGamecubeIPLSettings = new wxGridBagSizer();
	sGamecubeIPLSettings->Add(m_skip_bios_checkbox, wxGBPosition(0, 0), wxGBSpan(1, 2), wxALL, 5);
	sGamecubeIPLSettings->Add(new wxStaticText(this, wxID_ANY, _("System Language:")),
		wxGBPosition(1, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT | wxBOTTOM, 5);
	sGamecubeIPLSettings->Add(m_system_lang_choice, wxGBPosition(1, 1), wxDefaultSpan, wxLEFT | wxRIGHT | wxBOTTOM, 5);
	sGamecubeIPLSettings->Add(m_override_lang_checkbox, wxGBPosition(2, 0), wxGBSpan(1, 2), wxALL, 5);

	wxStaticBoxSizer* const sbGamecubeIPLSettings = new wxStaticBoxSizer(wxVERTICAL, this, _("IPL Settings"));
	sbGamecubeIPLSettings->Add(sGamecubeIPLSettings);
	wxStaticBoxSizer* const sbGamecubeDeviceSettings = new wxStaticBoxSizer(wxVERTICAL, this, _("Device Settings"));
	wxGridBagSizer* const sbGamecubeEXIDevSettings = new wxGridBagSizer(10, 10);

	for (int i = 0; i < 3; ++i)
	{
		sbGamecubeEXIDevSettings->Add(GCEXIDeviceText[i], wxGBPosition(i, 0), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);
		sbGamecubeEXIDevSettings->Add(m_exi_devices[i], wxGBPosition(i, 1), wxGBSpan(1, (i < 2) ? 1 : 2), wxALIGN_CENTER_VERTICAL);

		if (i < 2)
			sbGamecubeEXIDevSettings->Add(m_memcard_path[i], wxGBPosition(i, 2), wxDefaultSpan, wxALIGN_CENTER_VERTICAL);

		if (NetPlay::IsNetPlayRunning())
			m_exi_devices[i]->Disable();
	}
	sbGamecubeDeviceSettings->Add(sbGamecubeEXIDevSettings, 0, wxALL, 5);

	wxBoxSizer* const main_sizer = new wxBoxSizer(wxVERTICAL);
	main_sizer->Add(sbGamecubeIPLSettings, 0, wxEXPAND | wxALL, 5);
	main_sizer->Add(sbGamecubeDeviceSettings, 0, wxEXPAND | wxALL, 5);

	SetSizer(main_sizer);
}

void GameCubeConfigPane::LoadGUIValues()
{
	const SConfig& startup_params = SConfig::GetInstance();

	m_system_lang_choice->SetSelection(startup_params.SelectedLanguage);
	m_skip_bios_checkbox->SetValue(startup_params.bHLE_BS2);
	m_override_lang_checkbox->SetValue(startup_params.bOverrideGCLanguage);

	wxArrayString slot_devices;
	slot_devices.Add(_(DEV_NONE_STR));
	slot_devices.Add(_(DEV_DUMMY_STR));
	slot_devices.Add(_(EXIDEV_MEMCARD_STR));
	slot_devices.Add(_(EXIDEV_MEMDIR_STR));
	slot_devices.Add(_(EXIDEV_GECKO_STR));
	slot_devices.Add(_(EXIDEV_AGP_STR));

#if HAVE_PORTAUDIO
	slot_devices.Add(_(EXIDEV_MIC_STR));
#endif

	wxArrayString sp1_devices;
	sp1_devices.Add(_(DEV_NONE_STR));
	sp1_devices.Add(_(DEV_DUMMY_STR));
	sp1_devices.Add(_(EXIDEV_BBA_STR));
	sp1_devices.Add(_(EXIDEV_AM_BB_STR));

	for (int i = 0; i < 3; ++i)
	{
		bool isMemcard = false;

		// Add strings to the wxChoice list, the third wxChoice is the SP1 slot
		if (i == 2)
			m_exi_devices[i]->Append(sp1_devices);
		else
			m_exi_devices[i]->Append(slot_devices);

		switch (SConfig::GetInstance().m_EXIDevice[i])
		{
		case EXIDEVICE_NONE:
			m_exi_devices[i]->SetStringSelection(slot_devices[0]);
			break;
		case EXIDEVICE_MEMORYCARD:
			isMemcard = m_exi_devices[i]->SetStringSelection(slot_devices[2]);
			break;
		case EXIDEVICE_MEMORYCARDFOLDER:
			m_exi_devices[i]->SetStringSelection(slot_devices[3]);
			break;
		case EXIDEVICE_GECKO:
			m_exi_devices[i]->SetStringSelection(slot_devices[4]);
			break;
		case EXIDEVICE_AGP:
			isMemcard = m_exi_devices[i]->SetStringSelection(slot_devices[5]);
			break;
		case EXIDEVICE_MIC:
			m_exi_devices[i]->SetStringSelection(slot_devices[6]);
			break;
		case EXIDEVICE_ETH:
			m_exi_devices[i]->SetStringSelection(sp1_devices[2]);
			break;
		case EXIDEVICE_AM_BASEBOARD:
			m_exi_devices[i]->SetStringSelection(sp1_devices[3]);
			break;
		case EXIDEVICE_DUMMY:
		default:
			m_exi_devices[i]->SetStringSelection(slot_devices[1]);
			break;
		}

		if (!isMemcard && i < 2)
			m_memcard_path[i]->Disable();
	}
}

void GameCubeConfigPane::RefreshGUI()
{
	if (Core::IsRunning())
	{
		m_system_lang_choice->Disable();
		m_override_lang_checkbox->Disable();
		m_skip_bios_checkbox->Disable();
	}
}

void GameCubeConfigPane::OnSystemLanguageChange(wxCommandEvent& event)
{
	SConfig::GetInstance().SelectedLanguage = m_system_lang_choice->GetSelection();

	AddPendingEvent(wxCommandEvent(wxDOLPHIN_CFG_REFRESH_LIST));
}

void GameCubeConfigPane::OnOverrideLanguageCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().bOverrideGCLanguage = m_override_lang_checkbox->IsChecked();

	AddPendingEvent(wxCommandEvent(wxDOLPHIN_CFG_REFRESH_LIST));
}

void GameCubeConfigPane::OnSkipBiosCheckBoxChanged(wxCommandEvent& event)
{
	SConfig::GetInstance().bHLE_BS2 = m_skip_bios_checkbox->IsChecked();
}

void GameCubeConfigPane::OnSlotAChanged(wxCommandEvent& event)
{
	ChooseEXIDevice(event.GetString(), 0);
}

void GameCubeConfigPane::OnSlotBChanged(wxCommandEvent& event)
{
	ChooseEXIDevice(event.GetString(), 1);
}

void GameCubeConfigPane::OnSP1Changed(wxCommandEvent& event)
{
	ChooseEXIDevice(event.GetString(), 2);
}

void GameCubeConfigPane::OnSlotAButtonClick(wxCommandEvent& event)
{
	ChooseSlotPath(true, SConfig::GetInstance().m_EXIDevice[0]);
}

void GameCubeConfigPane::OnSlotBButtonClick(wxCommandEvent& event)
{
	ChooseSlotPath(false, SConfig::GetInstance().m_EXIDevice[1]);
}

void GameCubeConfigPane::ChooseEXIDevice(const wxString& deviceName, int deviceNum)
{
	TEXIDevices tempType;

	if (!deviceName.compare(_(EXIDEV_MEMCARD_STR)))
		tempType = EXIDEVICE_MEMORYCARD;
	else if (!deviceName.compare(_(EXIDEV_MEMDIR_STR)))
		tempType = EXIDEVICE_MEMORYCARDFOLDER;
	else if (!deviceName.compare(_(EXIDEV_MIC_STR)))
		tempType = EXIDEVICE_MIC;
	else if (!deviceName.compare(EXIDEV_BBA_STR))
		tempType = EXIDEVICE_ETH;
	else if (!deviceName.compare(EXIDEV_AGP_STR))
		tempType = EXIDEVICE_AGP;
	else if (!deviceName.compare(_(EXIDEV_AM_BB_STR)))
		tempType = EXIDEVICE_AM_BASEBOARD;
	else if (!deviceName.compare(EXIDEV_GECKO_STR))
		tempType = EXIDEVICE_GECKO;
	else if (!deviceName.compare(_(DEV_NONE_STR)))
		tempType = EXIDEVICE_NONE;
	else
		tempType = EXIDEVICE_DUMMY;

	// Gray out the memcard path button if we're not on a memcard or AGP
	if (tempType == EXIDEVICE_MEMORYCARD || tempType == EXIDEVICE_AGP)
		m_memcard_path[deviceNum]->Enable();
	else if (deviceNum == 0 || deviceNum == 1)
		m_memcard_path[deviceNum]->Disable();

	SConfig::GetInstance().m_EXIDevice[deviceNum] = tempType;

	if (Core::IsRunning())
	{
		// Change plugged device! :D
		ExpansionInterface::ChangeDevice(
			(deviceNum == 1) ? 1 : 0,  // SlotB is on channel 1, slotA and SP1 are on 0
			tempType,                  // The device enum to change to
			(deviceNum == 2) ? 2 : 0); // SP1 is device 2, slots are device 0
	}
}

void GameCubeConfigPane::ChooseSlotPath(bool is_slot_a, TEXIDevices device_type)
{
	bool memcard = (device_type == EXIDEVICE_MEMORYCARD);
	std::string path;
	std::string cardname;
	std::string ext;
	std::string pathA = SConfig::GetInstance().m_strMemoryCardA;
	std::string pathB = SConfig::GetInstance().m_strMemoryCardB;
	if (!memcard)
	{
		pathA = SConfig::GetInstance().m_strGbaCartA;
		pathB = SConfig::GetInstance().m_strGbaCartB;
	}
	SplitPath(is_slot_a ? pathA : pathB, &path, &cardname, &ext);
	std::string filename = WxStrToStr(wxFileSelector(
		_("Choose a file to open"),
		StrToWxStr(path),
		StrToWxStr(cardname),
		StrToWxStr(ext),
		memcard ? _("GameCube Memory Cards (*.raw,*.gcp)") + "|*.raw;*.gcp" : _("Game Boy Advance Carts (*.gba)") + "|*.gba"));

	if (!filename.empty())
	{
		if (File::Exists(filename))
		{
			if (memcard)
			{
				GCMemcard memorycard(filename);
				if (!memorycard.IsValid())
				{
					WxUtils::ShowErrorDialog(wxString::Format(_("Cannot use that file as a memory card.\n%s\n" \
						"is not a valid GameCube memory card file"), filename.c_str()));
					return;
				}
			}
		}
#ifdef _WIN32
		if (!strncmp(File::GetExeDirectory().c_str(), filename.c_str(), File::GetExeDirectory().size()))
		{
			// If the Exe Directory Matches the prefix of the filename, we still need to verify
			// that the next character is a directory separator character, otherwise we may create an invalid path
			char next_char = filename.at(File::GetExeDirectory().size()) + 1;
			if (next_char == '/' || next_char == '\\')
			{
				filename.erase(0, File::GetExeDirectory().size() + 1);
				filename = "./" + filename;
			}
		}
		std::replace(filename.begin(), filename.end(), '\\', '/');
#endif

		// also check that the path isn't used for the other memcard...
		if (filename.compare(is_slot_a ? pathB : pathA) != 0)
		{
			if (memcard)
			{
				if (is_slot_a)
					SConfig::GetInstance().m_strMemoryCardA = filename;
				else
					SConfig::GetInstance().m_strMemoryCardB = filename;
			}
			else
			{
				if (is_slot_a)
					SConfig::GetInstance().m_strGbaCartA = filename;
				else
					SConfig::GetInstance().m_strGbaCartB = filename;
			}

			if (Core::IsRunning())
			{
				// Change memcard to the new file
				ExpansionInterface::ChangeDevice(
					is_slot_a ? 0 : 1, // SlotA: channel 0, SlotB channel 1
					device_type,
					0); // SP1 is device 2, slots are device 0
			}
		}
		else
		{
			WxUtils::ShowErrorDialog(_("Are you trying to use the same file in both slots?"));
		}
	}
}
