#include "Classic.h"


namespace WiimoteEmu
{

static const u8 classic_id[]	= { 0x00, 0x00, 0xa4, 0x20, 0x01, 0x01 };
/* Classic Controller calibration */
static const u8 classic_calibration[] =
{
	0xff, 0x00, 0x80, 0xff, 0x00, 0x80,
	0xff, 0x00, 0x80, 0xff, 0x00, 0x80,
	0x00, 0x00, 0x51, 0xa6
};

// classic buttons
#define CLASSIC_PAD_RIGHT		0x80
#define CLASSIC_PAD_DOWN		0x40
#define CLASSIC_TRIGGER_L		0x20
#define CLASSIC_MINUS	 		0x10
#define	CLASSIC_HOME			0x08
#define CLASSIC_PLUS 			0x04
#define CLASSIC_TRIGGER_R		0x02
#define CLASSIC_NOTHING			0x01
#define CLASSIC_ZL				0x8000
#define CLASSIC_B				0x4000
#define CLASSIC_Y				0x2000
#define CLASSIC_A				0x1000
#define CLASSIC_X				0x0800
#define CLASSIC_ZR				0x0400
#define CLASSIC_PAD_LEFT		0x0200
#define CLASSIC_PAD_UP			0x0100


const u16 classic_button_bitmasks[] =
{
	CLASSIC_A,
	CLASSIC_B,
	CLASSIC_X,
	CLASSIC_Y,

	CLASSIC_ZL,
	CLASSIC_ZR,

	CLASSIC_MINUS,
	CLASSIC_PLUS,

	CLASSIC_HOME,
};

const char* classic_button_names[] =
{
	"A","B","X","Y","ZL","ZR","Minus","Plus","Home",
};

const u16 classic_trigger_bitmasks[] =
{
	CLASSIC_TRIGGER_L, CLASSIC_TRIGGER_R,
};

const char* const classic_trigger_names[] =
{
	"L", "R", "L-Analog", "R-Analog"
};

const u16 classic_dpad_bitmasks[] =
{
	CLASSIC_PAD_UP, CLASSIC_PAD_DOWN, CLASSIC_PAD_LEFT, CLASSIC_PAD_RIGHT 
};

Classic::Classic() : Attachment( "Classic Controller" )
{
	// buttons
	groups.push_back( m_buttons = new Buttons( "Buttons" ) );
	for ( unsigned int i = 0; i < sizeof(classic_button_names)/sizeof(*classic_button_names); ++i )
		m_buttons->controls.push_back( new ControlGroup::Input( classic_button_names[i] ) );

	// sticks
	groups.push_back( m_left_stick = new AnalogStick( "Left Stick" ) );
	groups.push_back( m_right_stick = new AnalogStick( "Right Stick" ) );

	// triggers
	groups.push_back( m_triggers = new MixedTriggers( "Triggers" ) );
	for ( unsigned int i=0; i < sizeof(classic_trigger_names)/sizeof(*classic_trigger_names); ++i )
		m_triggers->controls.push_back( new ControlGroup::Input( classic_trigger_names[i] ) );

	// dpad
	groups.push_back( m_dpad = new Buttons( "D-Pad" ) );
	for ( unsigned int i=0; i < 4; ++i )
		m_dpad->controls.push_back( new ControlGroup::Input( named_directions[i] ) );

	// set up register
	// calibration
	memcpy( &reg[0x20], classic_calibration, sizeof(classic_calibration) );
	// id
	memcpy( &reg[0xfa], classic_id, sizeof(classic_id) );
}

void Classic::GetState( u8* const data )
{
	wm_classic_extension* const ccdata = (wm_classic_extension*)data;

	// left stick
	{
	u8 x, y;
	m_left_stick->GetState( &x, &y, 0x20, 0x1F /*0x15*/ );

	ccdata->lx = x;
	ccdata->ly = y;
	}

	// right stick
	{
	u8 x, y;
	m_right_stick->GetState( &x, &y, 0x10, 0x0F /*0x0C*/ );

	ccdata->rx1 = x;
	ccdata->rx2 = x >> 1;
	ccdata->rx3 = x >> 3;
	ccdata->ry = y;
	}

	//triggers
	{
	u8 trigs[2];
	m_triggers->GetState( &ccdata->bt, classic_trigger_bitmasks, trigs, 0x1F );

	ccdata->lt1 = trigs[0];
	ccdata->lt2 = trigs[0] >> 3;
	ccdata->rt = trigs[1];
	}

	// buttons
	m_buttons->GetState( &ccdata->bt, classic_button_bitmasks );
	// dpad
	m_dpad->GetState( &ccdata->bt, classic_dpad_bitmasks );

	// flip button bits
	ccdata->bt ^= 0xFFFF;
}


}
