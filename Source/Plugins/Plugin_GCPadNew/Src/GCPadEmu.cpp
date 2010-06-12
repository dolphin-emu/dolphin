
#include "GCPadEmu.h"

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

	// rumble
	groups.push_back( m_rumble = new ControlGroup( "Rumble" ) );
	m_rumble->controls.push_back( new ControlGroup::Output( "Motor" ) );

	// dpad
	groups.push_back( m_dpad = new Buttons( "D-Pad" ) );
	for ( unsigned int i=0; i < 4; ++i )
		m_dpad->controls.push_back( new ControlGroup::Input( named_directions[i] ) );

	// options
	groups.push_back( m_options = new ControlGroup( "Options" ) );
	m_options->settings.push_back( new ControlGroup::Setting( "Background Input", false ) );

}

std::string GCPad::GetName() const
{
	return std::string("GCPad") + char('1'+m_index);
}

void GCPad::GetInput( SPADStatus* const pad )
{
	// if window has focus or background input enabled
	if (g_PADInitialize->pRendererHasFocus() || m_options[0].settings[0]->value )
	{
		// buttons
		m_buttons->GetState( &pad->button, button_bitmasks );

		// TODO: set analog A/B analog to full or w/e, prolly not needed

		// dpad
		m_dpad->GetState( &pad->button, dpad_bitmasks );

		// sticks
		m_main_stick->GetState( &pad->stickX, &pad->stickY, 0x80, 127 );
		m_c_stick->GetState( &pad->substickX, &pad->substickY, 0x80, 127 );

		// triggers
		m_triggers->GetState( &pad->button, trigger_bitmasks, &pad->triggerLeft, 0xFF );
	}
	else
	{
		// center sticks
		memset( &pad->stickX, 0x80, 4 );
	}
}

void GCPad::SetOutput( const bool on )
{
	// only rumble if window has focus or background input is enabled
	m_rumble->controls[0]->control_ref->State( on && (g_PADInitialize->pRendererHasFocus() || m_options[0].settings[0]->value) );
}

void GCPad::LoadDefaults()
{
	#define set_control(group, num, str)	(group)->controls[num]->control_ref->control_qualifier.name = (str)

	// nvm, do the device part elsewhere
//#ifdef _WIN32
//	default_device.FromString("DirectInput/0/Keyboard Mouse");
//#elif __APPLE__
//	// keyboard mouse devices are named by their product name thing on OSX currently
//#else
//	default_device.FromString("Xlib/0/Keyboard");
//#endif

	// Buttons
	set_control(m_buttons, 0, "X");		// A
	set_control(m_buttons, 1, "Z");		// B
	set_control(m_buttons, 2, "C");		// X
	set_control(m_buttons, 3, "S");		// Y
	set_control(m_buttons, 4, "D");		// Z
#ifdef _WIN32
	set_control(m_buttons, 5, "RETURN");	// Start
#else
	// osx/linux
	set_control(m_buttons, 5, "Return");	// Start
#endif

	// stick modifiers to 50 %
	m_main_stick->controls[4]->control_ref->range = 0.5f;
	m_c_stick->controls[4]->control_ref->range = 0.5f;

	// D-Pad
	set_control(m_dpad, 0, "T");		// Up
	set_control(m_dpad, 1, "G");		// Down
	set_control(m_dpad, 2, "F");		// Left
	set_control(m_dpad, 3, "H");		// Right

	// C-Stick
	set_control(m_c_stick, 0, "I");		// Up
	set_control(m_c_stick, 1, "K");		// Down
	set_control(m_c_stick, 2, "J");		// Left
	set_control(m_c_stick, 3, "L");		// Right
#ifdef _WIN32
	set_control(m_c_stick, 4, "LCONTROL");	// Modifier

	// Main Stick
	set_control(m_main_stick, 0, "UP");			// Up
	set_control(m_main_stick, 1, "DOWN");		// Down
	set_control(m_main_stick, 2, "LEFT");		// Left
	set_control(m_main_stick, 3, "RIGHT");		// Right
	set_control(m_main_stick, 4, "LSHIFT");		// Modifier

#elif __APPLE__
	set_control(m_c_stick, 4, "Left Control");	// Modifier

	// Main Stick
	set_control(m_main_stick, 0, "Up Arrow");		// Up
	set_control(m_main_stick, 1, "Down Arrow");		// Down
	set_control(m_main_stick, 2, "Left Arrow");		// Left
	set_control(m_main_stick, 3, "Right Arrow");	// Right
	set_control(m_main_stick, 4, "Left Shift");		// Modifier
#else
	// not sure if these are right

	set_control(m_c_stick, 4, "Control_L");	// Modifier

	// Main Stick
	set_control(m_main_stick, 0, "Up");		// Up
	set_control(m_main_stick, 1, "Down");		// Down
	set_control(m_main_stick, 2, "Left");		// Left
	set_control(m_main_stick, 3, "Right");	// Right
	set_control(m_main_stick, 4, "Shift_L");		// Modifier
#endif

	// Triggers
	set_control(m_triggers, 0, "Q");	// L
	set_control(m_triggers, 1, "W");	// R
}