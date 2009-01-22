// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef __CONFIG_MAIN_h__
#define __CONFIG_MAIN_h__

#include <wx/wx.h>
#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/filepicker.h>
#include "ConfigManager.h"

class CConfigMain
	: public wxDialog
{
	public:

		CConfigMain(wxWindow* parent,
			wxWindowID id = 1,
			const wxString& title = wxT("Dolphin Configuration"),
			const wxPoint& pos = wxDefaultPosition,
			const wxSize& size = wxDefaultSize,
			long style = wxDEFAULT_DIALOG_STYLE);
		virtual ~CConfigMain();
		void OnClick(wxMouseEvent& event);
		void CloseClick(wxCommandEvent& event);
		void OnSelectionChanged(wxCommandEvent& event);
		void OnConfig(wxCommandEvent& event);

		bool bRefreshList;
		bool bRefreshCache;

	private:

		DECLARE_EVENT_TABLE();

		wxBoxSizer* sGeneralPage; // General Settings
		wxCheckBox* ConfirmStop, * AutoHideCursor, *HideCursor;
		wxCheckBox* WiimoteStatusLEDs, * WiimoteStatusSpeakers;
		
		wxArrayString arrayStringFor_InterfaceLang;
		wxChoice* InterfaceLang;

		wxRadioBox* Theme;
		
		wxBoxSizer* sCore;
		wxStaticBoxSizer* sbBasic, *sbAdvanced, *sbInterface;
		wxCheckBox* AllwaysHLEBIOS;
		wxCheckBox* UseDynaRec;
		wxCheckBox* UseDualCore;
		wxCheckBox* LockThreads;
		wxCheckBox* OptimizeQuantizers;
		wxCheckBox* SkipIdle;
		wxCheckBox* EnableCheats;

		wxBoxSizer* sGamecube; // GC settings
		wxStaticBoxSizer* sbGamecubeIPLSettings;
		wxGridBagSizer* sGamecubeIPLSettings;
		wxArrayString arrayStringFor_GCSystemLang;
		wxStaticText* GCSystemLangText;
		wxChoice* GCSystemLang;		

		wxBoxSizer* sWii; // Wii settings
		wxStaticBoxSizer* sbWiimoteSettings;
		wxGridBagSizer* sWiimoteSettings;
		wxStaticBoxSizer* sbWiiIPLSettings;
		wxGridBagSizer* sWiiIPLSettings;
		wxBoxSizer* sPaths;
		wxStaticBoxSizer* sbISOPaths;		
		wxBoxSizer* sISOButtons;
		wxGridBagSizer* sOtherPaths;
		wxBoxSizer* sPlugins;
		wxStaticBoxSizer* sbGraphicsPlugin;
		wxStaticBoxSizer* sbDSPPlugin;
		wxStaticBoxSizer* sbPadPlugin;
		wxStaticBoxSizer* sbWiimotePlugin;

		wxNotebook *Notebook;
		wxPanel *GeneralPage;
		wxPanel *GamecubePage;
		wxPanel *WiiPage;
		wxPanel *PathsPage;
		wxPanel *PluginPage;

		wxButton* m_Close;


		FILE* pStream;
		u8 m_SYSCONF[0x4000];
		bool m_bSysconfOK;
		std::string FullSYSCONFPath;

		enum
		{
			BT_DINF = 0x0044,
			BT_SENS = 0x04AF,
			BT_BAR	= 0x04E1,
			BT_SPKV = 0x151A,
			BT_MOT = 0x1807
		};
		enum
		{
			IPL_AR = 0x04D9,
			IPL_SSV = 0x04EA,
			IPL_LNG = 0x04F3,
			IPL_PGS = 0x17CC,
			IPL_E60 = 0x17D5
		};
		/* Some offsets in SYSCONF: (taken from ector's SYSCONF)
		Note: offset is where the actual data starts, not where the type or name begins
		offset	length	name		comment
		0x0044	0x460	BT.DINF		List of Wiimotes "synced" to the system
		0x158B	?		BT.CDIF		? -- Starts with 0x0204 followed by 0x210 of 0x00
		0x04AF	4		BT.SENS		Wiimote sensitivity setting
		0x04E1	1		BT.BAR		Sensor bar position (0:bottom)
		0x151A	1		BT.SPKV		Wiimote speaker volume
		0x1807	1		BT.MOT		Wiimote motor on/off

		0x17F3	2		IPL.IDL		Shutdown mode and idle LED mode
		0x17C3	1		IPL.UPT		Update Type
		0x04BB	0x16	IPL.NIK		Console Nickname
		0x04D9	1		IPL.AR		Aspect ratio setting. 0: 4:3 1: 16:9
		0x04EA	1		IPL.SSV		Screen Saver off/on (burn-in reduction)
		0x04F3	1		IPL.LNG		System Language, see conf.c for some values
		0x04FD	0x1007	IPL.SADR	"Simple Address" Contains some region info
		0x150E	4		IPL.CB		Counter Bias -- difference between RTC and local time, in seconds
		0x1522	0x50	IPL.PC		Parental Control password/setting
		0x17B1	1		IPL.CD		Config Done flag -- has initial setup been performed?
		0x17BA	1		IPL.CD2		Config2 Done flag -- has network setup been performed?
		0x17FF	1		IPL.EULA	EULA Done flag -- has EULA been acknowledged?
		0x17CC	1		IPL.PGS		Use Progressive Scan
		0x17D5	1		IPL.E60		Use EuRGB60 (PAL6)
		?		1		IPL.SND		Sound setting
		0x17DD	1		IPL.DH		Display Offset (Horiz)
		0x179A	4		IPL.INC		"Installed Channel App Count"
		?		?		IPL.ARN		?
		0x17A7	4		IPL.FRC		"Free Channel App Count"

		?		?		DEV.BTM		?
		?		?		DEV.VIM		?
		?		?		DEV.CTC		?
		?		?		DEV.DSM		?
		?		?		DVD.CNF		?
		0x1582	?		WWW.RST		WWW Restriction
		?		?		NET.CNF		?
		?		?		NET.CFG		?
		0x1576	4		NET.CTPC	Net Content Restrictions ("Content Parental Control"?)
		0x17E7	4		NET.WCFG	WC24 Configuration flags
		*/
		wxArrayString arrayStringFor_WiiSensBarPos; // Wiimote Settings
		wxStaticText* WiiSensBarPosText;
		wxChoice* WiiSensBarPos;

		wxCheckBox* WiiScreenSaver; // IPL settings
		wxCheckBox* WiiProgressiveScan;
		wxCheckBox* WiiEuRGB60;
		wxArrayString arrayStringFor_WiiAspectRatio;
		wxStaticText* WiiAspectRatioText;
		wxChoice* WiiAspectRatio;
		wxArrayString arrayStringFor_WiiSystemLang;
		wxStaticText* WiiSystemLangText;
		wxChoice* WiiSystemLang;

		wxArrayString arrayStringFor_ISOPaths;
		wxListBox* ISOPaths;
		wxButton* AddISOPath;
		wxButton* RemoveISOPath;
		wxStaticText* DefaultISOText;
		wxFilePickerCtrl* DefaultISO;
		wxStaticText* DVDRootText;
		wxDirPickerCtrl* DVDRoot;

		wxStaticText* PADText;
		wxButton* PADConfig;
		wxChoice* PADSelection;
		wxButton* DSPConfig;
		wxStaticText* DSPText;
		wxChoice* DSPSelection;
		wxButton* GraphicConfig;
		wxStaticText* GraphicText;
		wxChoice* GraphicSelection;
		wxButton* WiimoteConfig;
		wxStaticText* WiimoteText;
		wxChoice* WiimoteSelection;

		enum
		{
			ID_NOTEBOOK = 1000,
			ID_GENERALPAGE,
			ID_GAMECUBEPAGE,
			ID_WIIPAGE,
			ID_PATHSPAGE,
			ID_PLUGINPAGE,
			ID_CLOSE,
			ID_ALLWAYS_HLEBIOS,
			ID_USEDYNAREC,
			ID_USEDUALCORE,
			ID_LOCKTHREADS,
			ID_OPTIMIZEQUANTIZERS,
			ID_IDLESKIP,
			ID_ENABLECHEATS,
			ID_ENABLEISOCACHE,
			ID_GC_SRAM_LNG_TEXT,
			ID_GC_SRAM_LNG,

			ID_INTERFACE_CONFIRMSTOP, // Interface settings
			ID_INTERFACE_HIDECURSOR_TEXT, ID_INTERFACE_HIDECURSOR, ID_INTERFACE_AUTOHIDECURSOR,
			ID_INTERFACE_WIIMOTE_TEXT, ID_INTERFACE_WIIMOTE_LEDS, ID_INTERFACE_WIIMOTE_SPEAKERS,
			ID_INTERFACE_LANG_TEXT, ID_INTERFACE_LANG,
			ID_INTERFACE_THEME,

			ID_WII_BT_BAR_TEXT,
			ID_WII_BT_BAR,

			ID_WII_IPL_SSV,
			ID_WII_IPL_PGS,
			ID_WII_IPL_E60,
			ID_WII_IPL_AR_TEXT,
			ID_WII_IPL_AR,
			ID_WII_IPL_LNG_TEXT,
			ID_WII_IPL_LNG,
			ID_ISOPATHS,
			ID_ADDISOPATH,
			ID_REMOVEISOPATH,
			ID_DEFAULTISO_TEXT,
			ID_DEFAULTISO,
			ID_DVDROOT_TEXT,
			ID_DVDROOT,
			ID_WIIMOTE_ABOUT,
			ID_WIIMOTE_CONFIG,
			ID_WIIMOTE_TEXT,
			ID_WIIMOTE_CB,
			ID_PAD_TEXT,
			ID_PAD_ABOUT ,
			ID_PAD_CONFIG,
			ID_PAD_CB,
			ID_DSP_ABOUT,
			ID_DSP_CONFIG,
			ID_DSP_TEXT,
			ID_DSP_CB,
			ID_GRAPHIC_ABOUT,
			ID_GRAPHIC_CONFIG,
			ID_GRAPHIC_TEXT,
			ID_GRAPHIC_CB
		};

		void CreateGUIControls(); void UpdateGUI();
		void OnClose(wxCloseEvent& event);
		void CoreSettingsChanged(wxCommandEvent& event);
		void InterfaceLanguageChanged(wxCommandEvent& event);
		void GCSettingsChanged(wxCommandEvent& event);
		void WiiSettingsChanged(wxCommandEvent& event);
		void ISOPathsSelectionChanged(wxCommandEvent& event);
		void AddRemoveISOPaths(wxCommandEvent& event);
		void DefaultISOChanged(wxFileDirPickerEvent& event);
		void DVDRootChanged(wxFileDirPickerEvent& event);

		void FillChoiceBox(wxChoice* _pChoice, int _PluginType, const std::string& _SelectFilename);

		void CallConfig(wxChoice* _pChoice);

		bool GetFilename(wxChoice* _pChoice, std::string& _rFilename);
};

#endif
