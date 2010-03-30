
#include "GCPad.h"

// this is all temporary, but sticking with the padspecs ...for now
const u16 button_bitmasks[] =
{
	PAD_BUTTON_A,
	PAD_BUTTON_B,
	PAD_BUTTON_X,
	PAD_BUTTON_Y,
	PAD_TRIGGER_Z,
	PAD_BUTTON_START
};

const u16 trigger_bitmasks[] =
{
	PAD_TRIGGER_L,
	PAD_TRIGGER_R,
};

const u16 dpad_bitmasks[] =
{
	PAD_BUTTON_UP, PAD_BUTTON_DOWN, PAD_BUTTON_LEFT, PAD_BUTTON_RIGHT 
};

const char* const named_buttons[] =
{
	"A",
	"B",
	"X",
	"Y",
	"Z",
	"Start",
};

const char* const named_triggers[] =
{
	"L", "R", "L-Analog", "R-Analog"
};

GCPad::GCPad( const unsigned int index ) : m_index(index)
{

	// buttons
	groups.push_back( m_buttons = new Buttons( "Buttons" ) );
	for ( unsigned int i=0; i < sizeof(named_buttons)/sizeof(*named_buttons); ++i )
		m_buttons->controls.push_back( new ControlGroup::Input( named_buttons[i] ) );

	// sticks
	groups.push_back( m_main_stick = new AnalogStick( "Main Stick" ) );
	groups.push_back( m_c_stick = new AnalogStick( "C-Stick" ) );

	// triggers
	groups.push_back( m_triggers = new MixedTriggers( "Triggers" ) );
	for ( unsigned int i=0; i < sizeof(named_triggers)/sizeof(*named_triggers); ++i )
		m_triggers->controls.push_back( new ControlGroup::Input( named_triggers[i] ) );

	// dpad
	groups.push_back( m_dpad = new Buttons( "D-Pad" ) );
	for ( unsigned int i=0; i < 4; ++i )
		m_dpad->controls.push_back( new ControlGroup::Input( named_directions[i] ) );

	// rumble
	groups.push_back( m_rumble = new ControlGroup( "Rumble" ) );
	m_rumble->controls.push_back( new ControlGroup::Output( "Motor" ) );

}

std::string GCPad::GetName() const
{
	return std::string("GCPad") + char('1'+m_index);
}

void GCPad::GetInput( SPADStatus* const pad )
{
	std::vector< ControlGroup::Control* >::iterator i,e;

	// buttons
	m_buttons->GetState( &pad->button, button_bitmasks );

	// TODO: set analog A/B to full or w/e

	// dpad
	m_dpad->GetState( &pad->button, dpad_bitmasks );

	// sticks
	m_main_stick->GetState( &pad->stickX, &pad->stickY, 0x80, 127 );
	m_c_stick->GetState( &pad->substickX, &pad->substickY, 0x80, 127 );

	// triggers
	m_triggers->GetState( &pad->button, trigger_bitmasks, &pad->triggerLeft, 0xFF );
}

void GCPad::SetOutput( const bool on )
{
	m_rumble->controls[0]->control_ref->State( on );
}
