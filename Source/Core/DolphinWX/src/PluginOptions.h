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

#ifndef __PLUGIN_OPTIONS_h__
#define __PLUGIN_OPTIONS_h__

#undef PLUGIN_OPTIONS_STYLE
#define PLUGIN_OPTIONS_STYLE wxCAPTION | wxSYSTEM_MENU | wxDIALOG_NO_PARENT | wxMINIMIZE_BOX | wxCLOSE_BOX

class CPluginOptions
	: public wxDialog
{
	private:

		DECLARE_EVENT_TABLE();

	public:

		CPluginOptions(wxWindow* parent, wxWindowID id = 1, const wxString& title = wxT("Untitled1"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = PLUGIN_OPTIONS_STYLE);
		virtual ~CPluginOptions();
		void OKClick(wxCommandEvent& event);
		void OnSelectionChanged(wxCommandEvent& event);
		void OnAbout(wxCommandEvent& event);
		void OnConfig(wxCommandEvent& event);


	private:

		//Do not add custom control declarations between
		//GUI Control Declaration Start and GUI Control Declaration End.
		//wxDev-C++ will remove them. Add custom code after the block.
		////GUI Control Declaration Start
		wxButton* OK;
		wxButton* Cancel;
		wxButton* Apply;
		wxStaticText* WxStaticText3;
		wxButton* PADAbout;
		wxButton* PADConfig;
		wxChoice* PADSelection;
		wxButton* DSPAbout;
		wxButton* DSPConfig;
		wxStaticText* WxStaticText2;
		wxChoice* DSPSelection;
		wxButton* GraphicAbout;
		wxButton* GraphicConfig;
		wxStaticText* WxStaticText1;
		wxChoice* GraphicSelection;
		////GUI Control Declaration End

	private:

		//Note: if you receive any error with these enum IDs, then you need to
		//change your old form code that are based on the #define control IDs.
		//#defines may replace a numeric value for the enum names.
		//Try copy and pasting the below block in your old form header files.
		enum
		{
			////GUI Enum Control ID Start
			ID_CANCEL = 1034,
			ID_APPLY = 1033,
			ID_OK = 1032,
			ID_WXSTATICTEXT3 = 1031,
			ID_PAD_ABOUT  = 1030,
			ID_PAD_CONFIG = 1029,
			ID_PAD_CB = 1028,
			ID_DSP_ABOUT  = 1027,
			ID_DSP_CONFIG = 1026,
			ID_WXSTATICTEXT2 = 1025,
			ID_DSP_CB = 1024,
			ID_GRAPHIC_ABOUT  = 1007,
			ID_GRAPHIC_CONFIG = 1006,
			ID_WXSTATICTEXT1 = 1005,
			ID_GRAPHIC_CB = 1003,
			////GUI Enum Control ID End
			ID_DUMMY_VALUE_ //don't remove this value unless you have other enum values
		};

	private:

		void OnClose(wxCloseEvent& event);
		void CreateGUIControls();

		void FillChoiceBox(wxChoice* _pChoice, int _PluginType, const std::string& _SelectFilename);

		void CallConfig(wxChoice* _pChoice);
		void CallAbout(wxChoice* _pChoice);

		void DoApply();

		bool GetFilename(wxChoice* _pChoice, std::string& _rFilename);
};

#endif
