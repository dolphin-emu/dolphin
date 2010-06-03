#ifndef _CONFIGBOX_H_
#define _CONFIGBOX_H_

#define SLIDER_TICK_COUNT			100
#define DETECT_WAIT_TIME			1500
#define PREVIEW_UPDATE_TIME			25

// might have to change this setup for wiimote
#define PROFILES_PATH				"Profiles/"

#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>

#include <sstream>
#include <vector>

#include <ControllerInterface/ControllerInterface.h>
#include <Config.h>
#include "FileSearch.h"

class PadSetting
{
protected:
	PadSetting() {}

public:
	virtual void UpdateGUI() = 0;
	virtual void UpdateValue() = 0;
};

class PadSettingExtension : public wxChoice, public PadSetting
{
public:
	PadSettingExtension( wxWindow* const parent, ControllerEmu::Extension* const ext );
	void UpdateGUI();
	void UpdateValue();

	ControllerEmu::Extension* const	extension;
};

class PadSettingChoice : public wxChoice, public PadSetting
{
public:
	PadSettingChoice( wxWindow* const parent, ControllerEmu::ControlGroup::Setting* const setting );
	void UpdateGUI();
	void UpdateValue();

	ControlState&		value;
};

class PadSettingCheckBox : public wxCheckBox, public PadSetting
{
public:
	PadSettingCheckBox( wxWindow* const parent, ControlState& _value, const char* const label );
	void UpdateGUI();
	void UpdateValue();

	ControlState&		value;
};

class ControlChooser : public wxStaticBoxSizer
{
public:
	ControlChooser( wxWindow* const parent, ControllerInterface::ControlReference* const ref, wxWindow* const eventsink );
	
	void UpdateGUI();
	void UpdateListContents();
	void UpdateListSelection();

	ControllerInterface::ControlReference*	control_reference;

	wxTextCtrl*			textctrl;
	wxListBox*			control_lbox;
	wxChoice*			mode_cbox;
	wxSlider*			range_slider;

private:
	wxStaticText*		m_bound_label;
};

class ControlList : public wxDialog
{
public:

	ControlList( wxWindow* const parent, ControllerInterface::ControlReference* const ref, ControlChooser* const chooser );

private:
	ControlChooser* const		m_control_chooser;
};

class ControlDialog : public wxDialog
{
public:
	ControlDialog( wxWindow* const parent, ControllerInterface::ControlReference* const ref, const std::vector<ControllerInterface::Device*>& devs );
	
	void SelectControl( wxCommandEvent& event );
	void DetectControl( wxCommandEvent& event );
	void ClearControl( wxCommandEvent& event );
	void SetControl( wxCommandEvent& event );
	void SetDevice( wxCommandEvent& event );

	ControllerInterface::ControlReference* const		control_reference;
	wxComboBox*				device_cbox;
	ControlChooser*			control_chooser;
};

class ExtensionButton : public wxButton
{
public:
	ExtensionButton( wxWindow* const parent, ControllerEmu::Extension* const ext )
		: wxButton( parent, -1, wxT("Configure"), wxDefaultPosition )
		, extension(ext) {}

	ControllerEmu::Extension* const	extension;
};

class ControlButton : public wxButton
{
public:
	ControlButton( wxWindow* const parent, ControllerInterface::ControlReference* const _ref, const unsigned int width, const std::string& label = "" );

	ControllerInterface::ControlReference* const		control_reference;
};

class ControlGroupBox : public wxStaticBoxSizer
{
public:
	ControlGroupBox( ControllerEmu::ControlGroup* const group, wxWindow* const parent, wxWindow* const eventsink );

	ControllerEmu::ControlGroup*	control_group;
	wxStaticBitmap*					static_bitmap;
	std::vector< PadSetting* >		options;
	std::vector< wxButton* >		controls;
	std::vector<ControlButton*>		control_buttons;	
};

class ControlGroupsSizer : public wxBoxSizer
{
public:
	ControlGroupsSizer( ControllerEmu* const controller, wxWindow* const parent,  wxWindow* const eventsink, std::vector<ControlGroupBox*>* const groups = NULL );

	
};

class ConfigDialog;

class GamepadPage : public wxNotebookPage
{
	friend class ConfigDialog;

public:
	GamepadPage( wxWindow* parent, const unsigned int pad_num, ConfigDialog* const config_dialog );

	void UpdateGUI();

	void RefreshDevices( wxCommandEvent& event );

	void LoadProfile( wxCommandEvent& event );
	void SaveProfile( wxCommandEvent& event );
	void DeleteProfile( wxCommandEvent& event );

	void ConfigControl( wxCommandEvent& event );
	void DetectControl( wxCommandEvent& event );

	void ConfigExtension( wxCommandEvent& event );

	void SetDevice( wxCommandEvent& event );

	void ClearAll( wxCommandEvent& event );

	void AdjustControlOption( wxCommandEvent& event );
	void AdjustSetting( wxCommandEvent& event );

	wxComboBox*					profile_cbox;
	wxComboBox*					device_cbox;

	std::vector<ControlGroupBox*>		control_groups;

protected:
	
	ControllerEmu* const				controller;

private:

	ControlDialog*				m_control_dialog;
	ConfigDialog* const			m_config_dialog;
};

class ConfigDialog : public wxDialog
{
public:

	ConfigDialog( wxWindow* const parent, Plugin& plugin, const std::string& name, const bool _is_game_running );
	~ConfigDialog();

	void ClickSave( wxCommandEvent& event );

	void UpdateDeviceComboBox();
	void UpdateProfileComboBox();

	void UpdateControlReferences();
	void UpdateBitmaps(wxTimerEvent&);

	const bool		is_game_running;

private:

	wxNotebook*					m_pad_notebook;
	std::vector<GamepadPage*>	m_padpages;
	Plugin&						m_plugin;
	wxTimer*					m_update_timer;
};

#endif
