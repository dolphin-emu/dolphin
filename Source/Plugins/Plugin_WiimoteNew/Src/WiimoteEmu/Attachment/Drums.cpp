#include "Drums.h"


namespace WiimoteEmu
{

static const u8 drums_id[] = { 0x01, 0x00, 0xa4, 0x20, 0x01, 0x03 };

// drums buttons
#define DRUMS_PLUS			0x04
#define DRUMS_MINUS			0x10

#define DRUMS_BASS			0x0400
#define DRUMS_BLUE			0x0800
#define DRUMS_GREEN			0x1000
#define DRUMS_YELLOW		0x2000
#define DRUMS_RED			0x4000
#define DRUMS_ORANGE		0x8000

const u16 drum_pad_bitmasks[] =
{
	DRUMS_RED,
	DRUMS_BLUE,
	DRUMS_GREEN,
	DRUMS_YELLOW,
	DRUMS_ORANGE,
	DRUMS_BASS,
};

const char* drum_pad_names[] =
{
	"Red","Blue","Green","Yellow","Orange","Bass"
};

const u16 drum_button_bitmasks[] =
{
	DRUMS_MINUS,
	DRUMS_PLUS,
};

Drums::Drums() : Attachment( "Drums" )
{
	// pads
	groups.push_back( m_pads = new Buttons( "Pads" ) );
	for ( unsigned int i = 0; i < sizeof(drum_pad_names)/sizeof(*drum_pad_names); ++i )
		m_pads->controls.push_back( new ControlGroup::Input( drum_pad_names[i] ) );

	// stick
	groups.push_back( m_stick = new AnalogStick( "Stick" ) );

	// buttons
	groups.push_back( m_buttons = new Buttons( "Buttons" ) );
	m_buttons->controls.push_back( new ControlGroup::Input("Minus") );
	m_buttons->controls.push_back( new ControlGroup::Input("Plus") );

	// set up register
	// id
	memcpy( &reg[0xfa], drums_id, sizeof(drums_id) );
}

void Drums::GetState(u8* const data, const bool focus)
{
	wm_drums_extension* const ddata = (wm_drums_extension*)data;

	// calibration data not figured out yet?

	// stick
	{
	u8 x, y;
	m_stick->GetState( &x, &y, 0x20, focus ? 0x1F /*0x15*/ : 0 );

	ddata->sx = x;
	ddata->sy = y;
	}

	// TODO: softness maybe
	data[2] = 0xFF;
	data[3] = 0xFF;

	if (focus)
	{
		// buttons
		m_buttons->GetState( &ddata->bt, drum_button_bitmasks );
		// pads
		m_pads->GetState( &ddata->bt, drum_pad_bitmasks );
	}

	// flip button bits
	ddata->bt ^= 0xFFFF;
}

}
