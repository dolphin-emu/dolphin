#include "ControllerEmu.h"

#include "ControllerEmu/GCPad/GCPad.h"

const char modifier[] = "Modifier";

// TODO: make this use iterators
ControllerEmu::~ControllerEmu()
{
	for ( unsigned int g = 0; g<groups.size(); ++g )
		delete groups[g];
}
// TODO: make this use iterators
ControllerEmu::ControlGroup::~ControlGroup()
{
	for ( unsigned int i = 0; i < controls.size(); ++i )
		delete controls[i];
	for ( unsigned int i = 0; i < settings.size(); ++i )
		delete settings[i];
}

// TODO: make this use iterators
void ControllerEmu::UpdateReferences( ControllerInterface& devi )
{
	for ( unsigned int g = 0; g<groups.size(); ++g )
		for ( unsigned int i = 0; i < groups[g]->controls.size(); ++i )
			devi.UpdateReference( groups[g]->controls[i]->control_ref );
}

void ControllerEmu::UpdateDefaultDevice()
{
	std::vector<ControlGroup*>::const_iterator i = groups.begin(),
		e = groups.end();
	for ( ; i!=e; ++i )
	{
		std::vector<ControlGroup::Control*>::const_iterator ci = (*i)->controls.begin(),
			ce = (*i)->controls.end();
			for ( ; ci!=ce; ++ci )
				(*ci)->control_ref->device_qualifier = default_device;
	}
}

void ControllerEmu::LoadConfig( IniFile::Section& sec )
{
	const std::string default_dev =  sec[ "Device" ];
	default_device.FromString( default_dev );

	std::vector<ControlGroup*>::const_iterator i = groups.begin(),
		e = groups.end();
	for ( ; i!=e; ++i )
	{
		std::string group( (*i)->name ); group += "/";

		// settings
		std::vector<ControlGroup::Setting*>::const_iterator si = (*i)->settings.begin(),
			se = (*i)->settings.end();
		for ( ; si!=se; ++si )
			(*si)->value = sec.Get(group+(*si)->name, (*si)->default_value*100) / 100;

		// controls
		std::vector<ControlGroup::Control*>::const_iterator ci = (*i)->controls.begin(),
			ce = (*i)->controls.end();
		for ( ; ci!=ce; ++ci )
		{
			// control and dev qualifier
			(*ci)->control_ref->control_qualifier.name = sec[group + (*ci)->name];
			(*ci)->control_ref->device_qualifier.FromString( sec.Get( group+(*ci)->name+"/Device", default_dev ) );

			// range
			(*ci)->control_ref->range = sec.Get( group+(*ci)->name+"/Range", 100.0f ) / 100;

			// input mode
			if ( (*ci)->control_ref->is_input )
				((ControllerInterface::InputReference*)((*ci)->control_ref))->mode
					= sec.Get( group+(*ci)->name+"/Mode", 0 );

		}
	}
}

void ControllerEmu::SaveConfig( IniFile::Section& sec )
{
	const std::string default_dev = default_device.ToString();
	sec[ " Device" ] = default_dev;

	std::vector<ControlGroup*>::const_iterator i = groups.begin(),
		e = groups.end();
	for ( ; i!=e; ++i )
	{
		std::string group( (*i)->name ); group += "/";

		// settings
		std::vector<ControlGroup::Setting*>::const_iterator si = (*i)->settings.begin(),
			se = (*i)->settings.end();
		for ( ; si!=se; ++si )
			sec.Set( group+(*si)->name, (*si)->value*100, (*si)->default_value*100 );

		// controls
		std::vector<ControlGroup::Control*>::const_iterator ci = (*i)->controls.begin(),
			ce = (*i)->controls.end();
		for ( ; ci!=ce; ++ci )
		{
			// control and dev qualifier
			sec[group + (*ci)->name] = (*ci)->control_ref->control_qualifier.name;
			sec.Set( group+(*ci)->name+"/Device", (*ci)->control_ref->device_qualifier.ToString(), default_dev );

			// range
			sec.Set( group+(*ci)->name+"/Range", (*ci)->control_ref->range*100, 100 );

			// input mode
			if ( (*ci)->control_ref->is_input )
				sec.Set( group+(*ci)->name+"/Mode",
					((ControllerInterface::InputReference*)((*ci)->control_ref))->mode, (unsigned int)0 );
		}
	}
}

ControllerEmu::AnalogStick::AnalogStick( const char* const _name ) : ControlGroup( _name, GROUP_TYPE_STICK )
{
	for ( unsigned int i = 0; i < 4; ++i )
		controls.push_back( new Input( named_directions[i] ) );

	controls.push_back( new Input( modifier ) );

	settings.push_back( new Setting("Dead Zone", 0 ) );
	settings.push_back( new Setting("Square Stick", 0 ) );

}

ControllerEmu::Buttons::Buttons( const char* const _name ) : ControlGroup( _name, GROUP_TYPE_BUTTONS )
{
	settings.push_back( new Setting("Threshold", 0.5f ) );
}

ControllerEmu::MixedTriggers::MixedTriggers( const char* const _name ) : ControlGroup( _name, GROUP_TYPE_MIXED_TRIGGERS )
{
	settings.push_back( new Setting("Threshold", 0.9f ) );
}
