
#include "ConfigDiag.h"

#define _connect_macro_( b, f, c, s )	(b)->Connect( wxID_ANY, (c), wxCommandEventHandler( f ), (wxObject*)0, (wxEvtHandler*)s );

static Plugin* g_plugin;

void GamepadPage::ConfigExtension( wxCommandEvent& event )
{
	ControllerEmu::Extension* const ex = ((ExtensionButton*)event.GetEventObject())->extension;

	// show config diag, if "none" isn't selected
	if ( ex->switch_extension )
	{
		wxDialog* const dlg = new wxDialog( this, -1, wxString::FromAscii(ex->attachments[ex->switch_extension]->GetName().c_str()), wxDefaultPosition );
		wxPanel* const pnl = new wxPanel( dlg, -1, wxDefaultPosition );
		wxBoxSizer* const pnl_szr = new wxBoxSizer( wxHORIZONTAL );

		const std::size_t orig_size = control_groups.size();

		ControlGroupsSizer* const szr = new ControlGroupsSizer( ex->attachments[ex->switch_extension], pnl, this, &control_groups );
		pnl->SetSizerAndFit( szr );	// needed
		pnl_szr->Add( pnl, 0, wxLEFT, 5 );
		dlg->SetSizerAndFit( pnl_szr );	// needed

		dlg->Center();

		dlg->ShowModal();
		dlg->Destroy();

		// remove the new groups that were just added, now that the window closed
		control_groups.resize( orig_size );
	}
}

PadSettingExtension::PadSettingExtension( wxWindow* const parent, ControllerEmu::Extension* const ext )
	: wxChoice( parent, -1 )
	, extension(ext)
{

	std::vector<ControllerEmu*>::const_iterator
		i = extension->attachments.begin(),
		e = extension->attachments.end();

	for ( ; i!=e; ++i )
		Append( wxString::FromAscii( (*i)->GetName().c_str() ) );

	UpdateGUI();
}

void PadSettingExtension::UpdateGUI()
{
	Select( extension->switch_extension );
}

void PadSettingExtension::UpdateValue()
{
	extension->switch_extension = GetSelection();
}

PadSettingCheckBox::PadSettingCheckBox( wxWindow* const parent, ControlState& _value, const char* const label )
	: wxCheckBox( parent, -1, wxString::FromAscii( label ), wxDefaultPosition )
	, value(_value)
{
	UpdateGUI();
}

void PadSettingCheckBox::UpdateGUI()
{
	SetValue( value > 0 );
}

void PadSettingCheckBox::UpdateValue()
{
	// 0.01 so its saved to the ini file as just 1. :(
	value = 0.01 * GetValue();
}

PadSettingChoice::PadSettingChoice( wxWindow* const parent, ControllerEmu::ControlGroup::Setting* const setting )
	: wxChoice( parent, -1, wxDefaultPosition, wxSize( 54, -1 ) )
	, value(setting->value)
{
	Append( wxT("0") );
	for ( unsigned int i = setting->low; i<=setting->high; ++i )
	{
		std::ostringstream ss;
		ss << i;
		Append( wxString::FromAscii( ss.str().c_str() ) );
	}

	UpdateGUI();
}

void PadSettingChoice::UpdateGUI()
{
	std::ostringstream ss;
	ss << int(value * 100);
	SetSelection( FindString( wxString::FromAscii( ss.str().c_str() ) ) );
}

void PadSettingChoice::UpdateValue()
{
	value = float( atoi( GetStringSelection().mb_str() ) ) / 100;
}

ControlDialog::ControlDialog( wxWindow* const parent, ControllerInterface::ControlReference* const ref, const std::vector<ControllerInterface::Device*>& devs )
	:wxDialog( parent, -1, wxT("Configure Control"), wxDefaultPosition )
	,control_reference(ref)
{

	device_cbox = new wxComboBox( this, -1, wxString::FromAscii( ref->device_qualifier.ToString().c_str() ), wxDefaultPosition, wxSize(256,-1), wxArrayString(), wxTE_PROCESS_ENTER );

	_connect_macro_( device_cbox, ControlDialog::SetDevice, wxEVT_COMMAND_COMBOBOX_SELECTED, this );
	_connect_macro_( device_cbox, ControlDialog::SetDevice, wxEVT_COMMAND_TEXT_ENTER, this );

	std::vector< ControllerInterface::Device* >::const_iterator i = devs.begin(),
		e = devs.end();
	ControllerInterface::DeviceQualifier dq;
	for ( ; i!=e; ++i )
	{
		dq.FromDevice( *i );
		device_cbox->Append( wxString::FromAscii( dq.ToString().c_str() ) );
	}

	control_chooser = new ControlChooser( this, ref, parent );

	wxStaticBoxSizer* const d_szr = new wxStaticBoxSizer( wxVERTICAL, this, wxT("Device") );
	d_szr->Add( device_cbox, 0, wxEXPAND|wxALL, 5 );

	wxBoxSizer* const szr = new wxBoxSizer( wxVERTICAL );
	szr->Add( d_szr, 0, wxEXPAND|wxLEFT|wxRIGHT|wxTOP, 5 );
	szr->Add( control_chooser, 0, wxEXPAND|wxALL, 5 );

	SetSizerAndFit( szr );	// needed

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
	pname += PROFILES_PATH; pname += g_plugin->profile_name;

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
		(*i)->controller->UpdateReferences( g_plugin->controller_interface );
}

void ConfigDialog::ClickSave( wxCommandEvent& event )
{
	g_plugin->SaveConfig();
	Close();
}

void ControlChooser::UpdateListContents()
{
	control_lbox->Clear();

	// make sure it's a valid device
	if ( control_reference->device )
	{
		if ( control_reference->is_input )
		{
			// for inputs
			std::vector<ControllerInterface::Device::Input*>::const_iterator
			   	i = control_reference->device->Inputs().begin(),
				e = control_reference->device->Inputs().end();
			for ( ; i!=e; ++i )
				control_lbox->Append( wxString::FromAscii( (*i)->GetName().c_str() ) );
		}
		else
		{
			// for outputs
			std::vector<ControllerInterface::Device::Output*>::const_iterator
			   	i = control_reference->device->Outputs().begin(),
				e = control_reference->device->Outputs().end();
			for ( ; i!=e; ++i )
				control_lbox->Append( wxString::FromAscii( (*i)->GetName().c_str() ) );
		}
	}

	UpdateListSelection();
}

void ControlChooser::UpdateListSelection()
{
	UpdateGUI();

	wxArrayString control_names = control_lbox->GetStrings();
	const std::string cname = control_reference->control_qualifier.name;

	control_lbox->DeselectAll();

	// if text starts and ends with '|' it's multiple controls, otherwise just single one
	if (cname.size() ? ((*cname.rbegin()=='|') && (*cname.begin()=='|')) : false)
	{
		for (int i = int(control_names.size()) - 1; i >= 0; --i)
			if (cname.find( control_names[i].Prepend(wxT('|')).Append(wxT('|')).ToAscii()) != cname.npos)
				control_lbox->Select( i );
	}
	else
	{
		const int n = control_lbox->FindString(wxString::FromAscii(cname.c_str()));
		if (n >= 0)
			control_lbox->Select(n);
	}
}

void ControlChooser::UpdateGUI()
{
	// update textbox
	textctrl->SetValue(wxString::FromAscii(control_reference->control_qualifier.name.c_str()));

	// updates the "bound controls:" label
	size_t bound = control_reference->controls.size();
	std::ostringstream ss;
	ss << "Bound Controls: ";
	if ( bound ) ss << bound; else ss << "None";
	m_bound_label->SetLabel( wxString::FromAscii(ss.str().c_str()) );
};

void GamepadPage::UpdateGUI()
{
	device_cbox->SetValue( wxString::FromAscii( controller->default_device.ToString().c_str() ) );

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
		std::vector<PadSetting*>::const_iterator si = (*g)->options.begin()
			, se = (*g)->options.end();
		for ( ; si!=se; ++si )
			(*si)->UpdateGUI();
	}
}

void GamepadPage::ClearAll( wxCommandEvent& event )
{
	g_plugin->controls_crit.Enter();		// enter

	// just load an empty ini section to clear everything :P
	IniFile::Section section;
	controller->LoadConfig(&section);

	// no point in using the real ControllerInterface i guess
	ControllerInterface face;
	controller->UpdateReferences(face);

	UpdateGUI();

	g_plugin->controls_crit.Leave();		// leave
}

void ControlDialog::SetControl( wxCommandEvent& event )
{
	control_reference->control_qualifier.name =
		std::string( control_chooser->textctrl->GetValue().ToAscii() );

	g_plugin->controls_crit.Enter();		// enter
	control_reference->UpdateControls();
	g_plugin->controls_crit.Leave();		// leave

	control_chooser->UpdateListSelection();
}

void GamepadPage::SetDevice( wxCommandEvent& event )
{
	controller->default_device.FromString( std::string( device_cbox->GetValue().ToAscii() ) );
	
	// show user what it was validated as
	device_cbox->SetValue( wxString::FromAscii( controller->default_device.ToString().c_str() ) );

	// this will set all the controls to this default device
	controller->UpdateDefaultDevice();

	// update references
	g_plugin->controls_crit.Enter();		// enter
	controller->UpdateReferences( g_plugin->controller_interface );
	g_plugin->controls_crit.Leave();		// leave
}

void ControlDialog::SetDevice( wxCommandEvent& event )
{
	control_reference->device_qualifier.FromString( std::string( device_cbox->GetValue().ToAscii() ) );
	
	// show user what it was validated as
	device_cbox->SetValue( wxString::FromAscii( control_reference->device_qualifier.ToString().c_str() ) );

	// update references
	g_plugin->controls_crit.Enter();		// enter
	g_plugin->controller_interface.UpdateReference( control_reference );
	g_plugin->controls_crit.Leave();		// leave

	// update gui
	control_chooser->UpdateListContents();
}

void ControlDialog::ClearControl( wxCommandEvent& event )
{
	control_reference->control_qualifier.name.clear();

	g_plugin->controls_crit.Leave();		// enter
	control_reference->UpdateControls();
	g_plugin->controls_crit.Leave();		// leave

	control_chooser->UpdateListSelection();
}

void GamepadPage::AdjustSetting( wxCommandEvent& event )
{
	g_plugin->controls_crit.Enter();		// enter

	// updates the setting value from the GUI control
	(dynamic_cast<PadSetting*>(event.GetEventObject()))->UpdateValue();

	g_plugin->controls_crit.Leave();		// leave
}

void GamepadPage::AdjustControlOption( wxCommandEvent& event )
{
	g_plugin->controls_crit.Enter();		// enter

	m_control_dialog->control_reference->range = ControlState( m_control_dialog->control_chooser->range_slider->GetValue() ) / SLIDER_TICK_COUNT;

	if ( m_control_dialog->control_reference->is_input )
		((ControllerInterface::InputReference*)m_control_dialog->control_reference)->mode =
			m_control_dialog->control_chooser->mode_cbox->GetSelection();

	g_plugin->controls_crit.Leave();		// leave
}

void GamepadPage::ConfigControl( wxCommandEvent& event )
{
	m_control_dialog  = new ControlDialog( this, ((ControlButton*)event.GetEventObject())->control_reference, g_plugin->controller_interface.Devices() );
	m_control_dialog->ShowModal();
	m_control_dialog->Destroy();

	// update changes that were made in the dialog
	UpdateGUI();
}

void ControlDialog::DetectControl( wxCommandEvent& event )
{
	// some hacks
	wxChar num = ((wxButton*)event.GetEventObject())->GetLabel()[0];
	if ( num > '9' )
		num = 1;
	else
		num -= 0x30;

	g_plugin->controls_crit.Enter();		// enter
	if ( control_reference->Detect( DETECT_WAIT_TIME, num ) )	// if we got input, update gui
		control_chooser->UpdateListSelection();
	g_plugin->controls_crit.Leave();		// leave
}

void GamepadPage::DetectControl( wxCommandEvent& event )
{
	ControlButton* btn = (ControlButton*)event.GetEventObject();

	g_plugin->controls_crit.Enter();		// enter

	btn->control_reference->Detect( DETECT_WAIT_TIME );
	btn->SetLabel( wxString::FromAscii( btn->control_reference->control_qualifier.name.c_str() ) );

	g_plugin->controls_crit.Leave();		// leave
}

void ControlDialog::SelectControl( wxCommandEvent& event )
{
	// needed for linux
	if (IsBeingDeleted())
		return;

	wxListBox* const lb = (wxListBox*)event.GetEventObject();

	wxArrayInt sels;
	lb->GetSelections( sels );
	wxArrayString names = lb->GetStrings();

	wxString final_label;

	if (sels.GetCount() == 1)
		final_label = names[ sels[0] ];
	else
	{
	final_label = wxT('|');
	for ( unsigned int i=0; i<sels.GetCount(); ++i )
		final_label += names[ sels[i] ] + wxT('|');
	}

	control_reference->control_qualifier.name =
		std::string( final_label.ToAscii() );
	
	g_plugin->controls_crit.Enter();		// enter
	control_reference->UpdateControls();
	g_plugin->controls_crit.Leave();		// leave

	control_chooser->UpdateGUI();
}

ControlChooser::ControlChooser( wxWindow* const parent, ControllerInterface::ControlReference* const ref, wxWindow* const eventsink )
: wxStaticBoxSizer( wxVERTICAL, parent, ref->is_input ? wxT("Input") : wxT("Output") )
	, control_reference(ref)
{
	textctrl = new wxTextCtrl( parent, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER );
	_connect_macro_( textctrl, ControlDialog::SetControl, wxEVT_COMMAND_TEXT_ENTER, parent);

	wxButton* const detect_button = new wxButton( parent, -1, ref->is_input ? wxT("Detect 1") : wxT("Test") );
	wxButton* const clear_button = new  wxButton( parent, -1, wxT("Clear"), wxDefaultPosition );
	wxButton* const set_button = new wxButton( parent, -1, wxT("Set")/*, wxDefaultPosition, wxSize( 32, -1 )*/ );


	control_lbox = new wxListBox( parent, -1, wxDefaultPosition, wxSize( 256, 128 ), wxArrayString(), wxLB_EXTENDED );
	_connect_macro_( control_lbox, ControlDialog::SelectControl, wxEVT_COMMAND_LISTBOX_SELECTED, parent);

	wxBoxSizer* const button_sizer = new wxBoxSizer( wxHORIZONTAL );
	button_sizer->Add( detect_button, 1, 0, 5 );
	if ( ref->is_input )
		for ( unsigned int i = 2; i<5; ++i )
		{
			wxButton* d_btn = new wxButton( parent, -1, wxChar( '0'+i ), wxDefaultPosition, wxSize(20,-1) );
			_connect_macro_( d_btn, ControlDialog::DetectControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);
			button_sizer->Add( d_btn );
		}
	button_sizer->Add( clear_button, 1, 0, 5 );
	button_sizer->Add( set_button, 1, 0, 5 );

	range_slider = new wxSlider( parent, -1, SLIDER_TICK_COUNT, 0, SLIDER_TICK_COUNT, wxDefaultPosition, wxDefaultSize, wxSL_TOP | wxSL_LABELS /*| wxSL_AUTOTICKS*/ );

	range_slider->SetValue( control_reference->range * SLIDER_TICK_COUNT );

	_connect_macro_( detect_button, ControlDialog::DetectControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);
	_connect_macro_( clear_button, ControlDialog::ClearControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);
	_connect_macro_( set_button, ControlDialog::SetControl, wxEVT_COMMAND_BUTTON_CLICKED, parent);

	_connect_macro_( range_slider, GamepadPage::AdjustControlOption, wxEVT_SCROLL_CHANGED, eventsink);
	wxStaticText* const range_label = new wxStaticText( parent, -1, wxT("Range"));
	m_bound_label = new wxStaticText( parent, -1, wxT("") );

	wxBoxSizer* const range_sizer = new wxBoxSizer( wxHORIZONTAL );
	range_sizer->Add( range_label, 0, wxCENTER|wxLEFT, 5 );
	range_sizer->Add( range_slider, 1, wxEXPAND|wxLEFT, 5 );

	wxBoxSizer* const txtbox_szr = new wxBoxSizer( wxHORIZONTAL );

	txtbox_szr->Add( textctrl, 1, wxEXPAND, 0 );


	Add( range_sizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 5 );
	if ( control_reference->is_input )
	{
		mode_cbox = new wxChoice( parent, -1 );
		mode_cbox->Append( wxT("OR") );
		mode_cbox->Append( wxT("AND") );
		mode_cbox->Append( wxT("NOT") );
		mode_cbox->Select( ((ControllerInterface::InputReference*)control_reference)->mode );

		_connect_macro_( mode_cbox, GamepadPage::AdjustControlOption, wxEVT_COMMAND_CHOICE_SELECTED, eventsink );
		
		wxBoxSizer* const mode_szr = new wxBoxSizer( wxHORIZONTAL );
		mode_szr->Add( new wxStaticText( parent, -1, wxT("Mode") ), 0, wxCENTER|wxLEFT|wxRIGHT, 5 );
		mode_szr->Add( mode_cbox, 0, wxLEFT, 5 );
		
		Add( mode_szr, 0, wxEXPAND|wxLEFT|wxRIGHT, 5 );
	}
	Add( txtbox_szr, 0, wxEXPAND|wxTOP|wxLEFT|wxRIGHT, 5 );
	Add( button_sizer, 0, wxEXPAND|wxBOTTOM|wxLEFT|wxRIGHT, 5 );
	Add( control_lbox, 0, wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM, 5 );
	Add( m_bound_label, 0, wxEXPAND|wxLEFT, 80 );

	UpdateListContents();
}

void GamepadPage::LoadProfile( wxCommandEvent& event )
{
	// TODO: check for dumb characters maybe
	if ( profile_cbox->GetValue().empty() )
		return;
	
	g_plugin->controls_crit.Enter();

	std::string fname( File::GetUserPath(D_CONFIG_IDX) );
	fname += PROFILES_PATH; fname += g_plugin->profile_name; fname += '/';
	fname += profile_cbox->GetValue().ToAscii(); fname += ".ini";

	if ( false == File::Exists( fname.c_str() ) )
		return;

	IniFile inifile;
	inifile.Load(fname);
	controller->LoadConfig( inifile.GetOrCreateSection("Profile"));
	controller->UpdateReferences( g_plugin->controller_interface );

	g_plugin->controls_crit.Leave();
	UpdateGUI();
}

void GamepadPage::SaveProfile( wxCommandEvent& event )
{
	// TODO: check for dumb characters
	if ( profile_cbox->GetValue().empty() )
		return;

	std::string fname( File::GetUserPath(D_CONFIG_IDX) );
	fname += PROFILES_PATH; fname += g_plugin->profile_name; fname += '/';
	if ( false == File::Exists( fname.c_str() ) )
		File::CreateFullPath( fname.c_str() );
	fname += profile_cbox->GetValue().ToAscii(); fname += ".ini";

	// don't need lock
	IniFile inifile;
	inifile.Load(fname);
	controller->SaveConfig( inifile.GetOrCreateSection("Profile") );
	inifile.Save(fname);

	m_config_dialog->UpdateProfileComboBox();
}

void GamepadPage::DeleteProfile( wxCommandEvent& event )
{
	// TODO: check for dumb characters maybe
	if ( profile_cbox->GetValue().empty() )
		return;
	
	// don't need lock
	std::string fname( File::GetUserPath(D_CONFIG_IDX) );
	fname += PROFILES_PATH; fname += g_plugin->profile_name; fname += '/';
	fname += profile_cbox->GetValue().ToAscii(); fname += ".ini";
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
		std::vector<ControllerInterface::Device*>::const_iterator di = g_plugin->controller_interface.Devices().begin(),
			de = g_plugin->controller_interface.Devices().end();
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
	g_plugin->controls_crit.Enter();		// enter

	// refresh devices
	g_plugin->controller_interface.DeInit();
	g_plugin->controller_interface.Init();

	// update all control references
	m_config_dialog->UpdateControlReferences();

	// update device cbox
	m_config_dialog->UpdateDeviceComboBox();

	g_plugin->controls_crit.Leave();		// leave
}

ControlGroupBox::ControlGroupBox( ControllerEmu::ControlGroup* const group, wxWindow* const parent, wxWindow* const eventsink )
: wxStaticBoxSizer( wxVERTICAL, parent, wxString::FromAscii( group->name ) )
{

	control_group = group;
	static_bitmap = NULL;

	wxFont m_SmallFont(7, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
	for ( unsigned int c = 0; c < group->controls.size(); ++c )
	{

		wxStaticText* const label = new wxStaticText( parent, -1, wxString::FromAscii( group->controls[c]->name )/*.append(wxT(" :"))*/ );
		
		ControlButton* const control_button = new ControlButton( parent,  group->controls[c]->control_ref, 80 );
		control_button->SetFont(m_SmallFont);
		ControlButton* adv_button = new ControlButton( parent, group->controls[c]->control_ref, 18, "+" );
		adv_button->SetFont(m_SmallFont);

		controls.push_back( control_button );
		control_buttons.push_back( control_button );

		_connect_macro_( control_button, GamepadPage::DetectControl, wxEVT_COMMAND_BUTTON_CLICKED, eventsink );
		_connect_macro_( adv_button, GamepadPage::ConfigControl, wxEVT_COMMAND_BUTTON_CLICKED, eventsink );

		wxBoxSizer* const control_sizer = new wxBoxSizer( wxHORIZONTAL );
		control_sizer->AddStretchSpacer( 1 );
		control_sizer->Add( label, 0, wxCENTER | wxRIGHT, 5 );
		control_sizer->Add( control_button, 0, 0, 0 );
		control_sizer->Add( adv_button, 0, 0, 5 );

		Add( control_sizer, 0, wxEXPAND|wxLEFT|wxRIGHT, 5 );

	}

	switch ( group->type )
	{
	case GROUP_TYPE_STICK :
	case GROUP_TYPE_TILT :
	case GROUP_TYPE_CURSOR :
	case GROUP_TYPE_FORCE :
		{
			wxBitmap bitmap(64, 64);
			wxMemoryDC dc;
			dc.SelectObject(bitmap);
			dc.Clear();
			dc.SelectObject(wxNullBitmap);

			static_bitmap = new wxStaticBitmap( parent, -1, bitmap, wxDefaultPosition, wxDefaultSize, wxBITMAP_TYPE_BMP );

			std::vector< ControllerEmu::ControlGroup::Setting* >::const_iterator
				i = group->settings.begin(),
				e = group->settings.end();

			wxBoxSizer* const szr = new wxBoxSizer( wxVERTICAL );
			for ( ; i!=e; ++i )
			{
				PadSettingChoice* cbox = new PadSettingChoice( parent, *i );
				_connect_macro_( cbox, GamepadPage::AdjustSetting, wxEVT_COMMAND_CHOICE_SELECTED, eventsink );
				options.push_back( cbox );
				szr->Add( new wxStaticText( parent, -1, wxString::FromAscii( (*i)->name ) ) );
				szr->Add( cbox, 0, wxLEFT, 0 );
			}

			wxBoxSizer* const h_szr = new wxBoxSizer( wxHORIZONTAL );
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

			PadSettingChoice* const threshold_cbox = new PadSettingChoice( parent, group->settings[0] );
			_connect_macro_( threshold_cbox, GamepadPage::AdjustSetting, wxEVT_COMMAND_CHOICE_SELECTED, eventsink );

			options.push_back( threshold_cbox );

			wxBoxSizer* const szr = new wxBoxSizer( wxHORIZONTAL );
			szr->Add( new wxStaticText( parent, -1, wxString::FromAscii( group->settings[0]->name ) ), 0, wxCENTER|wxRIGHT, 5 );
			szr->Add( threshold_cbox, 0, wxRIGHT, 5 );

			Add( szr, 0, wxALL|wxCENTER, 5 );
			Add( static_bitmap, 0, wxALL|wxCENTER, 5 );
		}
		break;
	case GROUP_TYPE_MIXED_TRIGGERS :
	case GROUP_TYPE_TRIGGERS :
		{
			int height = (int)(6 * group->controls.size());
			int width = 64+12+1;
			if (GROUP_TYPE_TRIGGERS == group->type)
			{
				height *= 2;
				width = 64;
			}
			wxBitmap bitmap(width, height+1);
			wxMemoryDC dc;
			dc.SelectObject(bitmap);
			dc.Clear();
			dc.SelectObject(wxNullBitmap);
			static_bitmap = new wxStaticBitmap( parent, -1, bitmap, wxDefaultPosition, wxDefaultSize, wxBITMAP_TYPE_BMP );

			std::vector<ControllerEmu::ControlGroup::Setting*>::const_iterator
				i = group->settings.begin(),
				e = group->settings.end();
			for ( ; i!=e; ++i )
			{
				PadSettingChoice* cbox = new PadSettingChoice( parent, *i );
				_connect_macro_( cbox, GamepadPage::AdjustSetting, wxEVT_COMMAND_CHOICE_SELECTED, eventsink );
				options.push_back( cbox );
				wxBoxSizer* const szr = new wxBoxSizer( wxHORIZONTAL );
				szr->Add( new wxStaticText( parent, -1, wxString::FromAscii( (*i)->name ) ), 0, wxCENTER|wxRIGHT, 5 );
				szr->Add( cbox, 0, wxRIGHT, 5 );
				Add( szr, 0, wxALL|wxCENTER, 5 );
			}

			Add( static_bitmap, 0, wxALL|wxCENTER, 5 );
		}
		break;
	case GROUP_TYPE_EXTENSION :
		{
			PadSettingExtension* const attachments = new PadSettingExtension( parent, (ControllerEmu::Extension*)group );
			wxButton* const configure = new ExtensionButton( parent, (ControllerEmu::Extension*)group );

			options.push_back( attachments );

			_connect_macro_( attachments, GamepadPage::AdjustSetting, wxEVT_COMMAND_CHOICE_SELECTED, eventsink );
			_connect_macro_( configure, GamepadPage::ConfigExtension, wxEVT_COMMAND_BUTTON_CLICKED, eventsink );

			Add( attachments, 0, wxTOP|wxLEFT|wxRIGHT|wxEXPAND, 5 );
			Add( configure, 0, wxALL|wxEXPAND, 5 );
		}
		break;
	default :
		{
			//options

			std::vector<ControllerEmu::ControlGroup::Setting*>::const_iterator
				i = group->settings.begin(),
				e = group->settings.end();
			for ( ; i!=e; ++i )
			{
				PadSettingCheckBox* setting_cbox = new PadSettingCheckBox( parent, (*i)->value, (*i)->name );
				_connect_macro_( setting_cbox, GamepadPage::AdjustSetting, wxEVT_COMMAND_CHECKBOX_CLICKED, eventsink );
				options.push_back( setting_cbox );

				Add( setting_cbox, 0, wxALL|wxLEFT, 5 );

			}
		}
		break;
	}

	//AddStretchSpacer( 0 );
}

ControlGroupsSizer::ControlGroupsSizer( ControllerEmu* const controller, wxWindow* const parent, wxWindow* const eventsink, std::vector<ControlGroupBox*>* groups )
	: wxBoxSizer( wxHORIZONTAL )
{

	wxBoxSizer* stacked_groups = NULL;
	for ( unsigned int i = 0; i < controller->groups.size(); ++i )
	{
		ControlGroupBox* control_group = new ControlGroupBox( controller->groups[i], parent, eventsink );

		if ( control_group->control_buttons.size() > 1 )
		{
			if ( stacked_groups )
				Add( stacked_groups, 0, /*wxEXPAND|*/wxBOTTOM|wxRIGHT, 5 );

			stacked_groups = new wxBoxSizer( wxVERTICAL );
			stacked_groups->Add( control_group, 0, wxEXPAND );
		}
		else
			stacked_groups->Add( control_group, 0, wxEXPAND );

		if ( groups )
			groups->push_back( control_group );
	}

	if ( stacked_groups )
		Add( stacked_groups, 0, /*wxEXPAND|*/wxBOTTOM|wxRIGHT, 5 );

}

GamepadPage::GamepadPage( wxWindow* parent, const unsigned int pad_num, ConfigDialog* const config_dialog )
	: wxNotebookPage( parent, -1 , wxDefaultPosition, wxDefaultSize )
	,controller(g_plugin->controllers[pad_num])
	,m_config_dialog(config_dialog)
{

	wxBoxSizer* control_group_sizer = new ControlGroupsSizer( g_plugin->controllers[pad_num], this, this, &control_groups );

	wxStaticBoxSizer* profile_sbox = new wxStaticBoxSizer( wxHORIZONTAL, this, wxT("Profile") );

	// device chooser

	wxStaticBoxSizer* const device_sbox = new wxStaticBoxSizer( wxHORIZONTAL, this, wxT("Device") );

	device_cbox = new wxComboBox( this, -1, wxT(""), wxDefaultPosition, wxSize(128,-1), 0, 0, wxTE_PROCESS_ENTER );

	wxButton* refresh_button = new wxButton( this, -1, wxT("Refresh"), wxDefaultPosition, wxSize(60,-1) );

	_connect_macro_( device_cbox, GamepadPage::SetDevice, wxEVT_COMMAND_COMBOBOX_SELECTED, this );
	_connect_macro_( device_cbox, GamepadPage::SetDevice, wxEVT_COMMAND_TEXT_ENTER, this );
	_connect_macro_( refresh_button, GamepadPage::RefreshDevices, wxEVT_COMMAND_BUTTON_CLICKED, this );

	device_sbox->Add( device_cbox, 1, wxLEFT|wxRIGHT, 5 );
	device_sbox->Add( refresh_button, 0, wxRIGHT|wxBOTTOM, 5 );

	wxStaticBoxSizer* const clear_sbox = new wxStaticBoxSizer( wxHORIZONTAL, this, wxT("Clear") );
	wxButton* all_button = new wxButton( this, -1, wxT("All"), wxDefaultPosition, wxSize(48,-1) );
	clear_sbox->Add( all_button, 1, wxLEFT|wxRIGHT, 5 );

	_connect_macro_(all_button, GamepadPage::ClearAll, wxEVT_COMMAND_BUTTON_CLICKED, this);

	profile_cbox = new wxComboBox( this, -1, wxT(""), wxDefaultPosition, wxSize(128,-1) );

	wxButton* const pload_btn = new wxButton( this, -1, wxT("Load"), wxDefaultPosition, wxSize(48,-1) );
	wxButton* const psave_btn = new wxButton( this, -1, wxT("Save"), wxDefaultPosition, wxSize(48,-1) );
	wxButton* const pdelete_btn = new wxButton( this, -1, wxT("Delete"), wxDefaultPosition, wxSize(60,-1) );

	_connect_macro_(pload_btn, GamepadPage::LoadProfile, wxEVT_COMMAND_BUTTON_CLICKED, this);
	_connect_macro_(psave_btn, GamepadPage::SaveProfile, wxEVT_COMMAND_BUTTON_CLICKED, this);
	_connect_macro_(pdelete_btn, GamepadPage::DeleteProfile, wxEVT_COMMAND_BUTTON_CLICKED, this);

	profile_sbox->Add( profile_cbox, 1, wxLEFT, 5 );
	profile_sbox->Add( pload_btn, 0, wxLEFT, 5 );
	profile_sbox->Add( psave_btn, 0, 0, 5 );
	profile_sbox->Add( pdelete_btn, 0, wxRIGHT|wxBOTTOM, 5 );

	wxBoxSizer* const dio = new wxBoxSizer( wxHORIZONTAL );
	dio->Add( device_sbox, 1, wxEXPAND|wxRIGHT, 5 );
	dio->Add( clear_sbox, 0, wxEXPAND|wxRIGHT, 5 );
	dio->Add( profile_sbox, 1, wxEXPAND|wxRIGHT, 5 );

	wxBoxSizer* const mapping = new wxBoxSizer( wxVERTICAL );

	mapping->Add( dio, 1, wxEXPAND|wxLEFT|wxTOP|wxBOTTOM, 5 );
	mapping->Add( control_group_sizer, 0, wxLEFT|wxEXPAND, 5 );

	UpdateGUI();

	SetSizerAndFit( mapping );	// needed
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
	g_plugin = &plugin;

	m_pad_notebook = new wxNotebook( this, -1, wxDefaultPosition, wxDefaultSize, wxNB_DEFAULT );
	for ( unsigned int i = 0; i < plugin.controllers.size(); ++i )
	{
		GamepadPage* gp = new GamepadPage( m_pad_notebook, i, this );
		m_padpages.push_back( gp );
		m_pad_notebook->AddPage( gp, wxString::FromAscii( g_plugin->gui_name ) + wxT(' ') + wxChar('1'+i) );
	}

	UpdateDeviceComboBox();
	UpdateProfileComboBox();

	wxButton* const close_button = new wxButton( this, -1, wxT("Save"));
	_connect_macro_(close_button, ConfigDialog::ClickSave, wxEVT_COMMAND_BUTTON_CLICKED, this);
	_connect_macro_(close_button, ConfigDialog::ClickSave, wxEVT_COMMAND_BUTTON_CLICKED, this);

	wxBoxSizer* btns = new wxBoxSizer( wxHORIZONTAL );
	//btns->Add( new wxStaticText( this, -1, wxString::FromAscii(ver.c_str())), 0, wxLEFT|wxTOP, 5 );
	btns->AddStretchSpacer();
	btns->Add( close_button, 0, 0, 0 );

	wxBoxSizer* const szr = new wxBoxSizer( wxVERTICAL );
	szr->Add( m_pad_notebook, 0, wxEXPAND|wxTOP|wxLEFT|wxRIGHT, 5 );
	szr->Add( btns, 0, wxEXPAND|wxALL, 5 );

	SetSizerAndFit( szr );	// needed

	// not needed here it seems, but it cant hurt
	//Layout();

	Center();

	// live preview update timer
	m_update_timer = new wxTimer( this, -1 );
	Connect( wxID_ANY, wxEVT_TIMER, wxTimerEventHandler( ConfigDialog::UpdateBitmaps ), (wxObject*)0, this );
	m_update_timer->Start( PREVIEW_UPDATE_TIME, wxTIMER_CONTINUOUS);


}
