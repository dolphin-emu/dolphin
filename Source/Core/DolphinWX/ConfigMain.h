// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <cstdio>
#include <string>

#include <wx/arrstr.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/string.h>
#include <wx/translation.h>
#include <wx/windowid.h>

#include "Common/CommonTypes.h"
#if defined(HAVE_XRANDR) && HAVE_XRANDR
#include "VideoBackends/OGL/GLInterface/X11Utils.h"
#endif

class wxBoxSizer;
class wxButton;
class wxCheckBox;
class wxChoice;
class wxDirPickerCtrl;
class wxFileDirPickerEvent;
class wxFilePickerCtrl;
class wxGridBagSizer;
class wxListBox;
class wxNotebook;
class wxPanel;
class wxRadioBox;
class wxSlider;
class wxSpinCtrl;
class wxSpinEvent;
class wxStaticBoxSizer;
class wxStaticText;
class wxWindow;

class CConfigMain : public wxDialog
{
public:

	CConfigMain(wxWindow* parent,
		wxWindowID id = 1,
		const wxString& title = _("Dolphin Configuration"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		long style = wxDEFAULT_DIALOG_STYLE);
	virtual ~CConfigMain();

	void OnOk(wxCommandEvent& event);
	void CloseClick(wxCommandEvent& event);
	void OnSelectionChanged(wxCommandEvent& event);
	void OnConfig(wxCommandEvent& event);
	void SetSelectedTab(int tab);

	bool bRefreshList;

	enum
	{
		ID_NOTEBOOK = 1000,
		ID_GENERALPAGE,
		ID_DISPLAYPAGE,
		ID_AUDIOPAGE,
		ID_GAMECUBEPAGE,
		ID_WIIPAGE,
		ID_PATHSPAGE,
	};

private:
	enum
	{
		ID_CPUTHREAD = 1010,
		ID_IDLESKIP,
		ID_ENABLECHEATS,
		ID_FRAMELIMIT,

		ID_CPUENGINE,
		ID_DSPTHREAD,

		ID_NTSCJ,

		// Audio Settings
		ID_DSPENGINE,
		ID_ENABLE_HLE_AUDIO,
		ID_ENABLE_THROTTLE,
		ID_DPL2DECODER,
		ID_LATENCY,
		ID_BACKEND,
		ID_VOLUME,

		// Interface settings
		ID_INTERFACE_CONFIRMSTOP,
		ID_INTERFACE_USEPANICHANDLERS,
		ID_INTERFACE_ONSCREENDISPLAYMESSAGES,
		ID_INTERFACE_LANG,
		ID_HOTKEY_CONFIG,

		ID_GC_SRAM_LNG,
		ID_GC_ALWAYS_HLE_BS2,

		ID_GC_EXIDEVICE_SLOTA,
		ID_GC_EXIDEVICE_SLOTA_PATH,
		ID_GC_EXIDEVICE_SLOTB,
		ID_GC_EXIDEVICE_SLOTB_PATH,
		ID_GC_EXIDEVICE_SP1,
		ID_GC_SIDEVICE0,
		ID_GC_SIDEVICE1,
		ID_GC_SIDEVICE2,
		ID_GC_SIDEVICE3,


		ID_WII_IPL_SSV,
		ID_WII_IPL_E60,
		ID_WII_IPL_AR,
		ID_WII_IPL_LNG,

		ID_WII_SD_CARD,
		ID_WII_KEYBOARD,


		ID_ISOPATHS,
		ID_RECURSIVEISOPATH,
		ID_ADDISOPATH,
		ID_REMOVEISOPATH,

		ID_DEFAULTISO,
		ID_DVDROOT,
		ID_APPLOADERPATH,
		ID_NANDROOT,

		ID_DSP_CB,
		ID_DSP_CONFIG,
		ID_DSP_ABOUT,
	};

	wxNotebook* Notebook;
	wxPanel* PathsPage;

	// Basic
	wxCheckBox* CPUThread;
	wxCheckBox* SkipIdle;
	wxCheckBox* EnableCheats;
	wxChoice* Framelimit;

	// Advanced
	wxRadioBox* CPUEngine;
	wxCheckBox* DSPThread;
	wxCheckBox* _NTSCJ;


	wxBoxSizer* sDisplayPage; // Display settings
	wxStaticBoxSizer* sbInterface; // Display and Interface sections


	// Audio
	wxBoxSizer* sAudioPage; // GC settings
	wxRadioBox* DSPEngine;
	wxSlider*   VolumeSlider;
	wxStaticText* VolumeText;
	wxCheckBox* DPL2Decoder;
	wxArrayString wxArrayBackends;
	wxChoice*   BackendSelection;
	wxSpinCtrl* Latency;

	// Interface
	wxCheckBox* ConfirmStop;
	wxCheckBox* UsePanicHandlers;
	wxCheckBox* OnScreenDisplayMessages;
	wxChoice* InterfaceLang;
	wxButton* HotkeyConfig;


	wxBoxSizer* sGamecubePage; // GC settings
	wxStaticBoxSizer* sbGamecubeIPLSettings;
	wxGridBagSizer* sGamecubeIPLSettings;

	// IPL
	wxChoice* GCSystemLang;
	wxCheckBox* GCAlwaysHLE_BS2;

	// Device
	wxChoice* GCEXIDevice[3];
	wxButton* GCMemcardPath[2];
	wxChoice* GCSIDevice[4];


	wxBoxSizer* sWiiPage; // Wii settings
	wxStaticBoxSizer* /*sbWiimoteSettings, **/sbWiiIPLSettings, *sbWiiDeviceSettings; // Wiimote, Misc and Device sections
	wxGridBagSizer* /*sWiimoteSettings, **/sWiiIPLSettings;

	// Misc
	wxCheckBox* WiiScreenSaver;
	wxCheckBox* WiiEuRGB60;
	wxChoice* WiiAspectRatio;
	wxChoice* WiiSystemLang;

	// Device
	wxCheckBox* WiiSDCard;
	wxCheckBox* WiiKeyboard;


	wxBoxSizer* sPathsPage; // Paths settings
	wxStaticBoxSizer* sbISOPaths;
	wxGridBagSizer* sOtherPaths;

	// ISO Directories
	wxListBox* ISOPaths;
	wxCheckBox* RecursiveISOPath;
	wxButton* AddISOPath;
	wxButton* RemoveISOPath;

	// DefaultISO, DVD Root, Apploader, NANDPath
	wxFilePickerCtrl* DefaultISO;
	wxDirPickerCtrl* DVDRoot;
	wxFilePickerCtrl* ApploaderPath;
	wxDirPickerCtrl* NANDRoot;

	// Graphics
	wxChoice* GraphicSelection;
	wxButton* GraphicConfig;

	wxButton* m_Ok;

	FILE* pStream;

	wxArrayString arrayStringFor_Framelimit;
	wxArrayString arrayStringFor_CPUEngine;
	wxArrayString arrayStringFor_DSPEngine;
	wxArrayString arrayStringFor_FullscreenResolution;
	wxArrayString arrayStringFor_InterfaceLang;
	wxArrayString arrayStringFor_GCSystemLang;
	wxArrayString arrayStringFor_WiiSensBarPos;
	wxArrayString arrayStringFor_WiiAspectRatio;
	wxArrayString arrayStringFor_WiiSystemLang;
	wxArrayString arrayStringFor_ISOPaths;

	void InitializeGUILists();
	void InitializeGUIValues();
	void InitializeGUITooltips();

	void CreateGUIControls();
	void UpdateGUI();
	void OnClose(wxCloseEvent& event);

	void CoreSettingsChanged(wxCommandEvent& event);

	void DisplaySettingsChanged(wxCommandEvent& event);
	void OnSpin(wxSpinEvent& event);

	void AudioSettingsChanged(wxCommandEvent& event);
	void AddAudioBackends();

	void GCSettingsChanged(wxCommandEvent& event);
	void ChooseMemcardPath(std::string& strMemcard, bool isSlotA);
	void ChooseSIDevice(wxString deviceName, int deviceNum);
	void ChooseEXIDevice(wxString deviceName, int deviceNum);

	void WiiSettingsChanged(wxCommandEvent& event);
	// Change from IPL.LNG value to country code
	inline u8 GetSADRCountryCode(int language);

	void ISOPathsSelectionChanged(wxCommandEvent& event);
	void RecursiveDirectoryChanged(wxCommandEvent& event);
	void AddRemoveISOPaths(wxCommandEvent& event);
	void DefaultISOChanged(wxFileDirPickerEvent& event);
	void DVDRootChanged(wxFileDirPickerEvent& event);
	void ApploaderPathChanged(wxFileDirPickerEvent& WXUNUSED (event));
	void NANDRootChanged(wxFileDirPickerEvent& event);

private:
	DECLARE_EVENT_TABLE();

	static bool SupportsVolumeChanges(std::string backend);
};
