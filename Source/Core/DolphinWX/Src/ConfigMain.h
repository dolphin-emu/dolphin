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

#include <wx/gbsizer.h>
#include <wx/notebook.h>
#include <wx/filepicker.h>
//#include <wx/listbox.h>

#undef CONFIG_MAIN_STYLE
#define CONFIG_MAIN_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxCLOSE_BOX

class CConfigMain
	: public wxDialog
{
	public:

		CConfigMain(wxWindow* parent, wxWindowID id = 1, const wxString& title = wxT("Dolphin Configuration"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = CONFIG_MAIN_STYLE);
		virtual ~CConfigMain();
		void OKClick(wxCommandEvent& event);
		void OnSelectionChanged(wxCommandEvent& event);
		void OnConfig(wxCommandEvent& event);

	private:

		DECLARE_EVENT_TABLE();
		
		wxGridBagSizer* sGeneral;
		wxGridBagSizer* sPaths;
		wxStaticBoxSizer* sbISOPaths;
		wxBoxSizer* sISOButtons;
		wxGridBagSizer* sPlugins;

		wxNotebook *Notebook;
		wxPanel *GeneralPage;
		wxPanel *PathsPage;
		wxPanel *PluginPage;

		wxButton* OK;
		wxButton* Cancel;
		wxButton* Apply;

		wxCheckBox* AllwaysHLEBIOS;
		wxCheckBox* UseDynaRec;
		wxCheckBox* UseDualCore;
		wxCheckBox* LockThreads;
		wxCheckBox* OptimizeQuantizers;
		wxCheckBox* SkipIdle;
		wxStaticText* ConsoleLangText;
		wxChoice* ConsoleLang;

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
			ID_PATHSPAGE,
			ID_PLUGINPAGE,
			ID_CANCEL,
			ID_APPLY,
			ID_OK,
			ID_ALLWAYS_HLEBIOS,
			ID_USEDYNAREC,
			ID_USEDUALCORE,
			ID_LOCKTHREADS,
			ID_OPTIMIZEQUANTIZERS,
			ID_IDLESKIP,
			ID_CONSOLELANG_TEXT,
			ID_CONSOLELANG,
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
			ID_DSP_ABOUT ,
			ID_DSP_CONFIG,
			ID_DSP_TEXT,
			ID_DSP_CB,
			ID_GRAPHIC_ABOUT ,
			ID_GRAPHIC_CONFIG,
			ID_GRAPHIC_TEXT,
			ID_GRAPHIC_CB,

			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values
		};

		void CreateGUIControls();
		void OnClose(wxCloseEvent& event);
		void AllwaysHLEBIOSCheck(wxCommandEvent& event);
		void UseDynaRecCheck(wxCommandEvent& event);
		void UseDualCoreCheck(wxCommandEvent& event);
		void LockThreadsCheck(wxCommandEvent& event);
		void OptimizeQuantizersCheck(wxCommandEvent& event);
		void SkipIdleCheck(wxCommandEvent& event);
		void ConsoleLangChanged(wxCommandEvent& event);
		void AddRemoveISOPaths(wxCommandEvent& event);
		void DefaultISOChanged(wxFileDirPickerEvent& event);
		void DVDRootChanged(wxFileDirPickerEvent& event);

		void FillChoiceBox(wxChoice* _pChoice, int _PluginType, const std::string& _SelectFilename);

		void CallConfig(wxChoice* _pChoice);
		//void CallAbout(wxChoice* _pChoice);

		void DoApply();

		bool GetFilename(wxChoice* _pChoice, std::string& _rFilename);
};

#endif
