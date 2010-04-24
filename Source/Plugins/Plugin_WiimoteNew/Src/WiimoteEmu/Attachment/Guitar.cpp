#include "Guitar.h"


namespace WiimoteEmu
{

static const u8 guitar_id[]	= { 0x00, 0x00, 0xa4, 0x20, 0x01, 0x03 };

// guitar buttons
#define GUITAR_PLUS			0x04
#define GUITAR_MINUS		0x10
#define GUITAR_BAR_DOWN		0x40

#define GUITAR_BAR_UP		0x0100
#define GUITAR_YELLOW		0x0800
#define GUITAR_GREEN		0x1000
#define GUITAR_BLUE			0x2000
#define GUITAR_RED			0x4000
#define GUITAR_ORANGE		0x8000

const u16 guitar_fret_bitmasks[] =
{
	GUITAR_GREEN,
	GUITAR_RED,
	GUITAR_YELLOW,
	GUITAR_BLUE,
	GUITAR_ORANGE,
};

const char* guitar_fret_names[] =
{
	"Green","Red","Yellow","Blue","Orange",
};

const u16 guitar_button_bitmasks[] =
{
	GUITAR_MINUS,
	GUITAR_PLUS,
};

const u16 guitar_strum_bitmasks[] =
{
	GUITAR_BAR_UP,
	GUITAR_BAR_DOWN,
};

Guitar::Guitar() : Attachment( "Guitar" )
{
	// frets
	groups.push_back( m_frets = new Buttons( "Frets" ) );
	for ( unsigned int i = 0; i < sizeof(guitar_fret_names)/sizeof(*guitar_fret_names); ++i )
		m_frets->controls.push_back( new ControlGroup::Input( guitar_fret_names[i] ) );

	// stick
	groups.push_back( m_stick = new AnalogStick( "Stick" ) );

	// strum
	groups.push_back( m_strum = new Buttons( "Strum" ) );
	m_strum->controls.push_back( new ControlGroup::Input("Up") );
	m_strum->controls.push_back( new ControlGroup::Input("Down") );

	// whammy
	groups.push_back( m_whammy = new Triggers( "Whammy" ) );
	m_whammy->controls.push_back( new ControlGroup::Input("Bar") );

	// buttons
	groups.push_back( m_buttons = new Buttons( "Buttons" ) );
	m_buttons->controls.push_back( new ControlGroup::Input("Minus") );
	m_buttons->controls.push_back( new ControlGroup::Input("Plus") );

	// set up register
	// id
	memcpy( &reg[0xfa], guitar_id, sizeof(guitar_id) );
}

void Guitar::GetState(u8* const data, const bool focus)
{
	wm_guitar_extension* const gdata = (wm_guitar_extension*)data;

	// calibration data not figured out yet?

	// stick
	{
	u8 x, y;
	m_stick->GetState( &x, &y, 0x20, focus ? 0x1F /*0x15*/ : 0 );

	gdata->sx = x;
	gdata->sy = y;
	}

	// TODO: touch bar, probably not
	gdata->tb = 0x0F;	// not touched

	// whammy bar
	u8 whammy;
	m_whammy->GetState( &whammy, 0x1F );
	gdata->whammy = whammy;

	if (focus)
	{
		// buttons
		m_buttons->GetState( &gdata->bt, guitar_button_bitmasks );
		// frets
		m_frets->GetState( &gdata->bt, guitar_fret_bitmasks );
		// strum
		m_strum->GetState( &gdata->bt, guitar_strum_bitmasks );
	}

	// flip button bits
	gdata->bt ^= 0xFFFF;
}

}
