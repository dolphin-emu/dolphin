
#include "ConfigDiag.h"

SettingCBox::SettingCBox( wxWindow* const parent, ControlState& _value, int min, int max )
	: wxChoice( parent, -1, wxDefaultPosition, wxSize( 48, -1 ) )
	, value(_value)
{
	Append( wxT("0") );
	for ( ; min<=max; ++min )
	{
		std::ostringstream ss;
		ss << min;
		Append( wxString::FromAscii( ss.str().c_str() ) );
	}

	std::ostringstream ss;
	ss << int(value * 100);
	SetSelection( FindString( wxString::FromAscii( ss.str().c_str() ) ) );

}

ControlDialog::ControlDialog( wxWindow* const parent, ControllerInterface::ControlReference* const ref, const std::vector<ControllerInterface::Device*>& devs )
	:wxDialog( parent, -1, wxT("Configure Control"), wxDefaultPosition )
	,control_reference(ref)
{

	device_cbox = new wxComboBox( this, -1, wxString::FromAscii( ref->device_qualifier.ToString().c_str() ), wxDefaultPosition, wxSize(256,-1), wxArrayString(), wxTE_PROCESS_ENTER );

	#define _connect_macro2_( b, f, c )		(b)->Connect( wxID_ANY, c, wxCommandEventHandler( GamepadPage::f ), (wxObject*)0, (wxEvtHandler*)parent );

	_connect_macro2_( device_cbox, SetDevice, wxEVT_COMMAND_COMBOBOX_SELECTED );
	_connect_macro2_( device_cbox, SetDevice, wxEVT_COMMAND_TEXT_ENTER );

	std::vector< ControllerInterface::Device* >::const_iterator i = devs.begin(),
		e = devs.end();
	ControllerInterface::DeviceQualifier dq;
	for ( ; i!=e; ++i )
	{
		dq.FromDevice( *i );
		device_cbox->Append( wxString::FromAscii( dq.ToString().c_str() ) );
	}

	control_chooser = new ControlChooser( this, ref, parent );

	wxStaticBoxSizer* d_szr = new wxStaticBoxSizer( wxVERTICAL, this, wxT("Device") );
	d_szr->Add( device_cbox, 0, wxEXPAND|wxALL, 5 );

	wxBoxSizer* szr = new wxBoxSizer( wxVERTICAL );
	szr->Add( d_szr, 0, wxEXPAND|wxLEFT|wxRIGHT|wxTOP, 5 );
	szr->Add( control_chooser, 0, wxEXPAND|wxALL, 5 );

	SetSizerAndFit( szr );

}

ControlButton::ControlButton( wxWindow* const parent, ControllerInterface::ControlReference* const _ref, const unsigned int width, const std::string& label )
: wxButton( parent, -1, wxT(""), wxDefaultPosition, wxSize( width,20) )
, control_reference( _ref )
{
	if ( label.empty() )
		SetLabel( wxString::FromAscii( _ref->control_qualifier.name.c_str() ) );
	else
		SetLabel( wxString::FromAscii( label.c_str() ) );
}

void ConfigDialog::UpdateProfileComboBox()
{
	std::string pname( File::GetUserPath(D_CONFIG_IDX) );
	pname += PROFILES_PATH;

	CFileSearch::XStringVector exts;
	exts.push_back("*.ini");
	CFileSearch::XStringVector dirs;
	dirs.push_back( pname );
	CFileSearch cfs( exts, dirs );
	const CFileSearch::XStringVector& sv = cfs.GetFileNames();

	wxArrayString strs;
	CFileSearch::XStringVector::const_iterator si = sv.begin(),
		se = sv.end();
	for ( ; si!=se; ++si )
	{
		std::string str( si->begin() + si->find_last_of('/') + 1 , si->end() - 4 ) ;
		strs.push_back( wxString::FromAscii( str.c_str() ) );
	}

	std::vector< GamepadPage* >::iterator i = m_padpages.begin(),
		e = m_padpages.end();
	for ( ; i != e; ++i  )
	{
		(*i)->profile_cbox->Clear();
		(*i)->profile_cbox->Append(strs);
	}
}

void ConfigDialog::UpdateControlReferences()
{
	std::vector< GamepadPage* >::iterator i = m_padpages.begin(),
		e = m_padpages.end();
	for ( ; i != e; ++i  )
		(*i)->controller->UpdateReferences( m_plugin.controller_interface );
}

void ConfigDialog::ClickSave( wxCommandEvent& event )
{
	m_plugin.SaveConfig();
	Close();
};

void ControlChooser::UpdateGUI()
{
	control_lbox->Clear();

	// make sure it's a valid device
	if ( control_reference->device )
	if ( control_reference->is_input )
	{
		// for inputs
		std::vector<ControllerInterface::Device::Input*>::const_iterator i = control_reference->device->Inputs().begin(),
			e = control_reference->device->Inputs().end();
		for ( ; i!=e; ++i )
			control_lbox->Append( wxString::FromAscii( (*i)->GetName().c_str() ) );
	}
	else
	{
		// for outputs
		std::vector<ControllerInterface::Device::Output*>::const_iterator i = control_reference->device->Outputs().begin(),
			e = control_reference->device->Outputs().end();
		for ( ; i!=e; ++i )
			control_lbox->Append( wxString::FromAscii( (*i)->GetName().c_str() ) );
	}

	// logic not 100% right here for a poorly formated qualifier
	// but its just for selecting crap in the listbox
	wxArrayString control_names = control_lbox->GetStrings();
	const std::string cname = control_reference->control_qualifier.name;
	for ( int i = int(control_names.size()) - 1; i >=0; --i )
	{
		if ( cname == std::string(control_names[i].ToAscii()) ||
			cname.find( control_names[i].Prepend(wxT('|')).Append(wxT('|')).ToAscii() ) != std::string::npos )
			control_lbox->Select( i );
		else
			control_lbox->Deselect( i );
	}

	size_t bound = control_reference->controls.size();
	std::ostringstream ss;
	ss << "Bound Controls: ";
	if ( bound ) ss << bound; else ss << "None";
	m_bound_label->SetLabel( wxString::FromAscii(ss.str().c_str()) );

	textctrl->SetLabel( wxString::FromAscii( control_reference->control_qualifier.name.c_str() ) );
};

void GamepadPage::UpdateGUI()
{
	device_cbox->SetLabel( wxString::FromAscii( controller->default_device.ToString().c_str() ) );

	std::vector< ControlGroupBox* >::const_iterator g = control_groups.begin(),
		ge = control_groups.end();
	for ( ; g!=ge; ++g )
	{
		// buttons
		std::vector<ControlButton*>::const_iterator i = (*g)->control_buttons.begin()
			, e = (*g)->control_buttons.end();
		for ( ; i!=e; ++i )
			(*i)->SetLabel( wxString::FromAscii( (*i)->control_reference->control_qualifier.name.c_str() ) );

		// cboxes
		std::vector<SettingCBox*>::const_iterator si = (*g)->options.begin()
			, se = (*g)->options.end();
		for ( ; si!=se; ++si )
		{
			std::ostringstream ss;
			ss << int((*si)->value * 100);
			(*si)->SetSelection( (*si)->FindString( wxString::FromAscii( ss.str().c_str() ) ) );
		}
	}
}

void GamepadPage::ClearAll( wxCommandEvent& event )
{
	m_plugin.controls_crit.Enter();		// enter

	// just load an empty ini section to clear everything :P
	IniSection Section = IniSection();
	ControllerInterface Face = ControllerInterface();
	controller->LoadConfig( Section );

	// no point in using the real ControllerInterface i guess
	controller->UpdateReferences( Face );

	UpdateGUI();

	m_plugin.controls_crit.Leave();		// leave
}

void GamepadPage::SetControl( wxCommandEvent& event )
{
	m_plugin.controls_crit.Enter();		// enter

	m_control_dialog->control_reference->control_qualifier.name = std::string( m_control_dialog->control_chooser->textctrl->GetLabel().ToAscii() );
	m_control_dialog->control_reference->UpdateControls();
	m_control_dialog->control_chooser->UpdateGUI();

	UpdateGUI();

	m_plugin.controls_crit.Leave();		// leave
}

void GamepadPage::SetDevice( wxCommandEvent& event )
{
	m_plugin.controls_crit.Enter();		// enter

	// TODO: need to handle the ConfigControl device in here

	// default device
	if ( event.GetEventObject() == device_cbox )
	{
		controller->default_device.FromString( std::string( device_cbox->GetValue().ToAscii() ) );
		
		// show user what it was validated as
		device_cbox->SetLabel( wxString::FromAscii( controller->default_device.ToString().c_str() ) );

		// this will set all the controls to this default device
		controller->UpdateDefaultDevice();

		// update references
		controller->UpdateReferences( m_plugin.controller_interface );
	}
	// control dialog
	else
	{
		m_control_dialog->control_reference->device_qualifier.FromString( std::string( m_control_dialog->device_cbox->GetValue().ToAscii() ) );
		
		m_control_dialog->device_cbox->SetLabel( wxString::FromAscii( m_control_dialog->control_reference->device_qualifier.ToString().c_str() ) );

		m_plugin.controller_interface.UpdateReference( m_control_dialog->control_reference );
		
		m_control_dialog->control_chooser->UpdateGUI();

	}

	m_plugin.controls_crit.Leave();		// leave

}

void GamepadPage::ClearControl( wxCommandEvent& event )
{
	m_control_dialog->control_reference->control_qualifier.name.clear();
	m_control_dialog->control_reference->UpdateControls();
	m_control_dialog->control_chooser->UpdateGUI();
	UpdateGUI();
}

void GamepadPage::AdjustSetting( wxCommandEvent& event )
{
	m_plugin.controls_crit.Enter();		// enter

	float setting = atoi( ((SettingCBox*)event.GetEventObject())->GetStringSelection().mb_str() );
	((SettingCBox*)event.GetEventObject())->value = setting / 100;

	m_plugin.controls_crit.Leave();		// leave
}

void GamepadPage::AdjustControlOption( wxCommandEvent& event )
{
	m_plugin.controls_crit.Enter();		// enter

	m_control_dialog->control_reference->range = ControlState( m_control_dialog->control_chooser->range_slider->GetValue() ) / SLIDER_TICK_COUNT;

	if ( m_control_dialog->control_reference->is_input )
	{
		((ControllerInterface::InputReference*)m_control_dialog->control_reference)->mode = m_control_dialog->control_chooser->mode_cbox->GetSelection();
	}

	m_plugin.controls_crit.Leave();		// leave
}

void GamepadPage::ConfigControl( wxCommandEvent& event )
{
	m_control_dialog  = new ControlDialog( this, ((ControlButton*)event.GetEventObject())->control_reference, m_plugin.controller_interface.Devices() );
	m_control_dialog->ShowModal();
	m_control_dialog->Destroy();
}

void GamepadPage::ConfigDetectControl( wxCommandEvent& event )
{
	m_plugin.controls_crit.Enter();		// enter

	// major major hacks
	wxChar num = ((wxButton*)event.GetEventObject())->GetLabel()[0];
	if ( num > '9' )
		num = 1;
	else
		num -= 0x30;

	if ( m_control_dialog->control_reference->Detect( DETECT_WAIT_TIME, num ) )	// if we got input, update control
	{
		UpdateGUI();
		m_control_dialog->control_chooser->UpdateGUI();
	}

	m_plugin.controls_crit.Leave();		// leave
}

void GamepadPage::DetectControl( wxCommandEvent& event )
{
	ControlButton* btn = (ControlButton*)event.GetEventObject();

	m_plugin.controls_crit.Enter();		// enter

	btn->control_reference->Detect( DETECT_WAIT_TIME );
	btn->SetLabel( wxString::FromAscii( btn->control_reference->control_qualifier.name.c_str() ) );

	m_plugin.controls_crit.Leave();		// leave
}

void ControlDialog::SelectControl( wxCommandEvent& event )
{
	wxListBox* lb = (wxListBox*)event.GetEventObject();

	wxArrayInt sels;
	lb->GetSelections( sels );
	wxArrayString names = lb->GetStrings();

	wxString final_label;

	if ( sels.size() == 1 )
		final_label = names[ sels[0] ];
	//else if ( sels.size() == lb->GetCount() )
	//	final_label = "||";
	else
	{
	final_label = wxT('|');
	for ( unsigned int i=0; i<sels.size(); ++i )
		final_label += names[ sels[i] ] + wxT('|');
	}

	control_chooser->textctrl->SetLabel( final_label );

	// kinda dumb
	wxCommandEvent nullevent;
	((GamepadPage*)m_parent)->SetControl( nullevent );
	
}

ControlChooser::ControlChooser( wxWindow* const parent, ControllerInterface::ControlReference* const ref, wxWindow* const eventsink )
: wxStaticBoxSizer( wxVERTICAL, parent, ref->is_input ? wxT("Input") : wxT("Output") )
	, control_reference(ref)
{
	#define _connect_macro_( b, f, c )		(b)->Connect( wxID_ANY, (c), wxCommandEventHandler( GamepadPage::f ), (wxObject*)0, (wxEvtHandler*)eventsink );

	textctrl = new wxTextCtrl( parent, -1 );
	wxButton* detect_button = new wxButton( parent, -1, ref->is_input ? wxT("Detect 1") : wxT("Test") );
	wxButton* clear_button = new  wxButton( parent, -1, wxT("Clear"), wxDefaultPosition );
	wxButton* set_button = new wxButton( parent, -1, wxT("Set")/*, wxDefaultPosition, wxSize( 32, -1 )*/ );

	control_lbox = new wxListBox( parent, -1, wxDefaultPosition, wxSize( 256, 128 ), wxArrayString(), wxLB_EXTENDED );

	control_lbox->Connect( wxID_ANY, wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler( ControlDialog::SelectControl ), (wxObject*)0, parent );

	wxBoxSizer* button_sizer = new wxBoxSizer( wxHORIZONTAL );
	button_sizer->Add( detect_button, 1, 0, 5 );
	if ( ref->is_input )
		for ( unsigned int i = 2; i<5; ++i )
		{
			wxButton* d_btn = new wxButton( parent, -1, wxChar( '0'+i ), wxDefaultPosition, wxSize(16,-1) );
			_connect_macro_( d_btn, ConfigDetectControl, wxEVT_COMMAND_BUTTON_CLICKED);
			button_sizer->Add( d_btn );
		}
	button_sizer->Add( clear_button, 1, 0, 5 );
	button_sizer->Add( set_button, 1, 0, 5 );

	range_slider = new wxSlider( parent, -1, SLIDER_TICK_COUNT, 0, SLIDER_TICK_COUNT, wxDefaultPosition, wxDefaultSize, wxSL_TOP | wxSL_LABELS /*| wxSL_AUTOTICKS*/ );

	range_slider->SetValue( control_reference->range * SLIDER_TICK_COUNT );

	_connect_macro_( detect_button, ConfigDetectControl, wxEVT_COMMAND_BUTTON_CLICKED);
	_connect_macro_( clear_button, ClearControl, wxEVT_COMMAND_BUTTON_CLICKED);
	_connect_macro_( set_button, SetControl, wxEVT_COMMAND_BUTTON_CLICKED);

	_connect_macro_( range_slider, AdjustControlOption, wxEVT_SCROLL_CHANGED);
	wxStaticText* range_label = new wxStaticText( parent, -1, wxT("Range"));
	m_bound_label = new wxStaticText( parent, -1, wxT("") );

	wxBoxSizer* range_sizer = new wxBoxSizer( wxHORIZONTAL );
	range_sizer->Add( range_label, 0, wxCENTER|wxLEFT, 5 );
	range_sizer->Add( range_slider, 1, wxEXPAND|wxLEFT, 5 );

	wxBoxSizer* txtbox_szr = new wxBoxSizer( wxHORIZONTAL );

	txtbox_szr->Add( textctrl, 1, wxEXPAND, 0 );

	wxBoxSizer* mode_szr;
	if ( control_reference->is_input )
	{
		mode_cbox = new wxChoice( parent, -1 );
		mode_cbox->Append( wxT("OR") );
		mode_cbox->Append( wxT("AND") );
		mode_cbox->Append( wxT("NOT") );
		mode_cbox->Select( ((ControllerInterface::InputReference*)control_reference)->mode );

		_connect_macro_( mode_cbox, AdjustControlOption, wxEVT_COMMAND_CHOICE_SELECTED );
		
		mode_szr = new wxBoxSizer( wxHORIZONTAL );
		mode_szr->Add( new wxStaticText( parent, -1, wxT("Mode") ), 0, wxCENTER|wxLEFT|wxRIGHT, 5 );
		mode_szr->Add( mode_cbox, 0, wxLEFT, 5 );
	}

	Add( range_sizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 5 );
	if ( control_reference->is_input )
		Add( mode_szr, 0, wxEXPAND|wxLEFT|wxRIGHT, 5 );
	Add( txtbox_szr, 0, wxEXPAND|wxTOP|wxLEFT|wxRIGHT, 5 );
	Add( button_sizer, 0, wxEXPAND|wxBOTTOM|wxLEFT|wxRIGHT, 5 );
	Add( control_lbox, 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5 );
	Add( m_bound_label, 0, wxEXPAND|wxLEFT, 80 );

	UpdateGUI();
}

void GamepadPage::LoadProfile( wxCommandEvent& event )
{
	// TODO: check for dumb characters maybe
	if ( profile_cbox->GetValue().empty() )
		return;
	
	m_plugin.controls_crit.Enter();

	std::ifstream file;
	std::string fname( File::GetUserPath(D_CONFIG_IDX) );
	fname += PROFILES_PATH; fname += profile_cbox->GetValue().ToAscii(); fname += ".ini";

	if ( false == File::Exists( fname.c_str() ) )
		return;

	file.open( fname.c_str() );
	IniFile inifile;
	inifile.Load( file );
	controller->LoadConfig( inifile["Profile"] );
	file.close();

	controller->UpdateReferences( m_plugin.controller_interface );

	m_plugin.controls_crit.Leave();

	UpdateGUI();
}

void GamepadPage::SaveProfile( wxCommandEvent& event )
{
	// TODO: check for dumb characters
	if ( profile_cbox->GetValue().empty() )
		return;

	// don't need lock
	IniFile inifile;
	controller->SaveConfig( inifile["Profile"] );
	std::ofstream file;
	std::string fname( File::GetUserPath(D_CONFIG_IDX) );
	fname += PROFILES_PATH;
	
	if ( false == File::Exists( fname.c_str() ) )
		File::CreateFullPath( fname.c_str() );

	fname += profile_cbox->GetValue().ToAscii(); fname += ".ini";

	file.open( fname.c_str() );
	inifile.Save( file );
	file.close();

	m_config_dialog->UpdateProfileComboBox();
}

void GamepadPage::DeleteProfile( wxCommandEvent& event )
{
	// TODO: check for dumb characters maybe
	if ( profile_cbox->GetValue().empty() )
		return;
	
	// don't need lock
	std::string fname( File::GetUserPath(D_CONFIG_IDX) );
	fname += PROFILES_PATH; fname += profile_cbox->GetValue().ToAscii(); fname += ".ini";
	if ( File::Exists( fname.c_str() ) )
		File::Delete( fname.c_str() );

	m_config_dialog->UpdateProfileComboBox();
}

void ConfigDialog::UpdateDeviceComboBox()
{
	std::vector< GamepadPage* >::iterator i = m_padpages.begin(),
		e = m_padpages.end();
	ControllerInterface::DeviceQualifier dq;
	for ( ; i != e; ++i  )
	{
		(*i)->device_cbox->Clear();
		std::vector<ControllerInterface::Device*>::const_iterator di = m_plugin.controller_interface.Devices().begin(),
			de = m_plugin.controller_interface.Devices().end();
		for ( ; di!=de; ++di )
		{
			dq.FromDevice( *di );
			(*i)->device_cbox->Append( wxString::FromAscii( dq.ToString().c_str() ) );
		}
		(*i)->device_cbox->SetValue( wxString::FromAscii( (*i)->controller->default_device.ToString().c_str() ) );
	}
}

void GamepadPage::RefreshDevices( wxCommandEvent& event )
{
	m_plugin.controls_crit.Enter();		// enter

	// refresh devices
	m_plugin.controller_interface.DeInit();
	m_plugin.controller_interface.Init();

	// update all control references
	m_config_dialog->UpdateControlReferences();

	// update device cbox
	m_config_dialog->UpdateDeviceComboBox();

	m_plugin.controls_crit.Leave();		// leave
}

ControlGroupBox::ControlGroupBox( ControllerEmu::ControlGroup* const group, wxWindow* const parent )
: wxStaticBoxSizer( wxVERTICAL, parent, wxString::FromAscii( group->name ) )
{

	control_group = group;
	static_bitmap = NULL;

	for ( unsigned int c = 0; c < group->controls.size(); ++c )
	{

		wxStaticText* label = new wxStaticText( parent, -1, wxString::FromAscii( group->controls[c]->name )/*.append(wxT(" :"))*/ );
		ControlButton* control_button = new ControlButton( parent,  group->controls[c]->control_ref, 80 );
		controls.push_back( control_button );
		ControlButton* adv_button = new ControlButton( parent, group->controls[c]->control_ref, 16, "+" );

		control_buttons.push_back( control_button );

		control_button->Connect( wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( GamepadPage::DetectControl ),
			(wxObject*)0, (wxEvtHandler*)parent );

		adv_button->Connect( wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( GamepadPage::ConfigControl ),
			(wxObject*)0, (wxEvtHandler*)parent );

		wxBoxSizer* control_sizer = new wxBoxSizer( wxHORIZONTAL );
		control_sizer->AddStretchSpacer( 1 );
		control_sizer->Add( label, 0, wxCENTER | wxRIGHT, 5 );
		control_sizer->Add( control_button, 0, 0, 0 );
		control_sizer->Add( adv_button, 0, 0, 5 );

		Add( control_sizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 5 );

	}

	switch ( group->type )
	{
	case GROUP_TYPE_STICK :
		{
			wxBitmap bitmap(64, 64);
			wxMemoryDC dc;
			dc.SelectObject(bitmap);
			dc.Clear();
			dc.SelectObject(wxNullBitmap);

			static_bitmap = new wxStaticBitmap( parent, -1, bitmap, wxDefaultPosition, wxDefaultSize, wxBITMAP_TYPE_BMP );

			SettingCBox* deadzone_cbox = new SettingCBox( parent, group->settings[0]->value, 1, 50 );
			SettingCBox* diagonal_cbox = new SettingCBox( parent, group->settings[1]->value, 1, 100 );

			deadzone_cbox->Connect( wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( GamepadPage::AdjustSetting ), (wxObject*)0, (wxEvtHandler*)parent );
			diagonal_cbox->Connect( wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( GamepadPage::AdjustSetting ), (wxObject*)0, (wxEvtHandler*)parent );

			options.push_back( deadzone_cbox );
			options.push_back( diagonal_cbox );

			wxBoxSizer* szr = new wxBoxSizer( wxVERTICAL );
			szr->Add( new wxStaticText( parent, -1, wxString::FromAscii( group->settings[0]->name ) ) );
			szr->Add( deadzone_cbox, 0, wxLEFT, 0 );
			szr->Add( new wxStaticText( parent, -1, wxString::FromAscii( group->settings[1]->name ) ) );
			szr->Add( diagonal_cbox, 0, wxLEFT, 0 );

			wxBoxSizer* h_szr = new wxBoxSizer( wxHORIZONTAL );
			h_szr->Add( szr, 1, 0, 5 );
			h_szr->Add( static_bitmap, 0, wxALL|wxCENTER, 5 );

			Add( h_szr, 0, wxEXPAND|wxLEFT|wxCENTER|wxTOP, 5 );
		}
		break;
	case GROUP_TYPE_BUTTONS :
		{
			wxBitmap bitmap(int(12*group->controls.size()+1), 12);
			wxMemoryDC dc;
			dc.SelectObject(bitmap);
			dc.Clear();
			dc.SelectObject(wxNullBitmap);
			static_bitmap = new wxStaticBitmap( parent, -1, bitmap, wxDefaultPosition, wxDefaultSize, wxBITMAP_TYPE_BMP );

			SettingCBox* threshold_cbox = new SettingCBox( parent, group->settings[0]->value, 1, 99 );
			threshold_cbox->Connect( wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( GamepadPage::AdjustSetting ), (wxObject*)0, (wxEvtHandler*)parent );

			options.push_back( threshold_cbox );

			wxBoxSizer* szr = new wxBoxSizer( wxHORIZONTAL );
			szr->Add( new wxStaticText( parent, -1, wxString::FromAscii( group->settings[0]->name ) ), 0, wxCENTER|wxRIGHT, 5 );
			szr->Add( threshold_cbox, 0, wxRIGHT, 5 );

			Add( szr, 0, wxALL|wxCENTER, 5 );
			Add( static_bitmap, 0, wxALL|wxCENTER, 5 );
		}
		break;
	case GROUP_TYPE_MIXED_TRIGGERS :
		{
			wxBitmap bitmap(64+12+1, int(6*group->controls.size())+1);
			wxMemoryDC dc;
			dc.SelectObject(bitmap);
			dc.Clear();
			dc.SelectObject(wxNullBitmap);
			static_bitmap = new wxStaticBitmap( parent, -1, bitmap, wxDefaultPosition, wxDefaultSize, wxBITMAP_TYPE_BMP );

			SettingCBox* threshold_cbox = new SettingCBox( parent, group->settings[0]->value, 1, 99 );
			threshold_cbox->Connect( wxID_ANY, wxEVT_COMMAND_CHOICE_SELECTED, wxCommandEventHandler( GamepadPage::AdjustSetting ), (wxObject*)0, (wxEvtHandler*)parent );

			options.push_back( threshold_cbox );

			wxBoxSizer* szr = new wxBoxSizer( wxHORIZONTAL );
			szr->Add( new wxStaticText( parent, -1, wxString::FromAscii( group->settings[0]->name ) ), 0, wxCENTER|wxRIGHT, 5 );
			szr->Add( threshold_cbox, 0, wxRIGHT, 5 );

			Add( szr, 0, wxALL|wxCENTER, 5 );
			Add( static_bitmap, 0, wxALL|wxCENTER, 5 );
		}
		break;
	default :
		break;

	}

	//AddStretchSpacer( 0 );
}

GamepadPage::GamepadPage( wxWindow* parent, Plugin& plugin, const unsigned int pad_num, ConfigDialog* const config_dialog )
	: wxNotebookPage( parent, -1 , wxDefaultPosition, wxDefaultSize )
	,controller(plugin.controllers[pad_num])
	,m_plugin(plugin)
	,m_config_dialog(config_dialog)
{

	wxBoxSizer* control_group_sizer = new wxBoxSizer( wxHORIZONTAL );

	wxBoxSizer* stacked_groups = NULL;
	for ( unsigned int i = 0; i < m_plugin.controllers[pad_num]->groups.size(); ++i )
	{
		ControlGroupBox* control_group = new ControlGroupBox( m_plugin.controllers[pad_num]->groups[i], this );

		if ( control_group->control_buttons.size() > 3 )
		{
			if ( stacked_groups )
				control_group_sizer->Add( stacked_groups, 0, /*wxEXPAND|*/wxBOTTOM|wxRIGHT, 5 );

			stacked_groups = new wxBoxSizer( wxVERTICAL );
			stacked_groups->Add( control_group, 1 );
		}
		else
			stacked_groups->Add( control_group );

		control_groups.push_back( control_group );
	}

	if ( stacked_groups )
		control_group_sizer->Add( stacked_groups, 0, /*wxEXPAND|*/wxBOTTOM|wxRIGHT, 5 );

	wxStaticBoxSizer* profile_sbox = new wxStaticBoxSizer( wxHORIZONTAL, this, wxT("Profile") );

	// device chooser

	wxStaticBoxSizer* device_sbox = new wxStaticBoxSizer( wxHORIZONTAL, this, wxT("Device") );

	device_cbox = new wxComboBox( this, -1, wxT(""), wxDefaultPosition, wxSize(128,-1), 0, 0, wxTE_PROCESS_ENTER );

	wxButton* refresh_button = new wxButton( this, -1, wxT("Refresh"), wxDefaultPosition, wxSize(48,-1) );

	#define _connect_macro3_( b, f, c )		(b)->Connect( wxID_ANY, c, wxCommandEventHandler( GamepadPage::f ), (wxObject*)0, (wxEvtHandler*)this );

	_connect_macro3_( device_cbox, SetDevice, wxEVT_COMMAND_COMBOBOX_SELECTED );
	_connect_macro3_( device_cbox, SetDevice, wxEVT_COMMAND_TEXT_ENTER );
	_connect_macro3_( refresh_button, RefreshDevices, wxEVT_COMMAND_BUTTON_CLICKED );

	device_sbox->Add( device_cbox, 1, wxLEFT|wxRIGHT, 5 );
	device_sbox->Add( refresh_button, 0, wxRIGHT|wxBOTTOM, 5 );

	wxStaticBoxSizer* clear_sbox = new wxStaticBoxSizer( wxHORIZONTAL, this, wxT("Clear") );
	wxButton* all_button = new wxButton( this, -1, wxT("All"), wxDefaultPosition, wxSize(48,-1) );
	clear_sbox->Add( all_button, 1, wxLEFT|wxRIGHT, 5 );

	all_button->Connect( wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( GamepadPage::ClearAll ),
		(wxObject*)0, (wxEvtHandler*)this );

	profile_cbox = new wxComboBox( this, -1, wxT(""), wxDefaultPosition, wxSize(128,-1) );

	wxButton* const pload_btn = new wxButton( this, -1, wxT("Load"), wxDefaultPosition, wxSize(48,-1) );
	wxButton* const psave_btn = new wxButton( this, -1, wxT("Save"), wxDefaultPosition, wxSize(48,-1) );
	wxButton* const pdelete_btn = new wxButton( this, -1, wxT("Delete"), wxDefaultPosition, wxSize(48,-1) );

	pload_btn->Connect( wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( GamepadPage::LoadProfile ),
		(wxObject*)0, (wxEvtHandler*)this );
	psave_btn->Connect( wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( GamepadPage::SaveProfile ),
		(wxObject*)0, (wxEvtHandler*)this );
	pdelete_btn->Connect( wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( GamepadPage::DeleteProfile ),
		(wxObject*)0, (wxEvtHandler*)this );

	profile_sbox->Add( profile_cbox, 1, wxLEFT, 5 );
	profile_sbox->Add( pload_btn, 0, wxLEFT, 5 );
	profile_sbox->Add( psave_btn, 0, 0, 5 );
	profile_sbox->Add( pdelete_btn, 0, wxRIGHT|wxBOTTOM, 5 );

	wxBoxSizer* dio = new wxBoxSizer( wxHORIZONTAL );
	dio->Add( device_sbox, 1, wxEXPAND|wxRIGHT, 5 );
	dio->Add( clear_sbox, 0, wxEXPAND|wxRIGHT, 5 );
	dio->Add( profile_sbox, 1, wxEXPAND|wxRIGHT, 5 );

	wxBoxSizer* mapping = new wxBoxSizer( wxVERTICAL );

	mapping->Add( dio, 1, wxEXPAND|wxLEFT|wxTOP|wxBOTTOM, 5 );
	mapping->Add( control_group_sizer, 0, wxLEFT|wxEXPAND, 5 );

	UpdateGUI();

	SetSizerAndFit( mapping );
	Layout();
};

ConfigDialog::~ConfigDialog()
{
	m_update_timer->Stop();
}

ConfigDialog::ConfigDialog( wxWindow* const parent, Plugin& plugin, const std::string& name, const bool _is_game_running )
	: wxDialog( parent, wxID_ANY, wxString::FromAscii(name.c_str()), wxPoint(128,-1), wxDefaultSize )
	, is_game_running(_is_game_running)
	, m_plugin(plugin)
{
	m_pad_notebook = new wxNotebook( this, -1, wxDefaultPosition, wxDefaultSize, wxNB_DEFAULT );
	for ( unsigned int i = 0; i < plugin.controllers.size(); ++i )
	{
		GamepadPage* gp = new GamepadPage( m_pad_notebook, plugin, i, this );
		m_padpages.push_back( gp );
		m_pad_notebook->AddPage( gp, wxString::FromAscii((plugin.controllers[i]->GetName().c_str())) );
	}

	UpdateDeviceComboBox();
	UpdateProfileComboBox();

	wxButton* close_button = new wxButton( this, -1, wxT("Save"));

	close_button->Connect( wxID_ANY, wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler( ConfigDialog::ClickSave ), (wxObject*)0, (wxEvtHandler*)this );

	wxBoxSizer* btns = new wxBoxSizer( wxHORIZONTAL );
	//btns->Add( new wxStaticText( this, -1, wxString::FromAscii(ver.c_str())), 0, wxLEFT|wxTOP, 5 );
	btns->AddStretchSpacer();
	btns->Add( close_button, 0, 0, 0 );

	wxBoxSizer* szr = new wxBoxSizer( wxVERTICAL );
	szr->Add( m_pad_notebook, 0, wxEXPAND|wxTOP|wxLEFT|wxRIGHT, 5 );
	szr->Add( btns, 0, wxEXPAND|wxALL, 5 );

	SetSizerAndFit( szr );
	// not needed here it seems, but it cant hurt
	Layout();

	Center();

	// live preview update timer
	m_update_timer = new wxTimer( this, -1 );
	Connect( wxID_ANY, wxEVT_TIMER, wxTimerEventHandler( ConfigDialog::UpdateBitmaps ), (wxObject*)0, this );
	m_update_timer->Start( PREVIEW_UPDATE_TIME, wxTIMER_CONTINUOUS);


}
