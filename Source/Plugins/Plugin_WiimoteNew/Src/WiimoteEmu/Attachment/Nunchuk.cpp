#include "Nunchuk.h"


namespace WiimoteEmu
{

static const u8 nunchuck_id[] = { 0x00, 0x00, 0xa4, 0x20, 0x00, 0x00 };
/* Default calibration for the nunchuck. It should be written to 0x20 - 0x3f of the
   extension register. 0x80 is the neutral x and y accelerators and 0xb3 is the
   neutral z accelerometer that is adjusted for gravity. */
static const u8 nunchuck_calibration[] =
{
	0x80, 0x80, 0x80, 0x00, // accelerometer x, y, z neutral
	0xb3, 0xb3, 0xb3, 0x00, //  x, y, z g-force values 

	// 0x80 = analog stick x and y axis center
	0xff, 0x00, 0x80,
	0xff, 0x00, 0x80,
	0xee, 0x43 // checksum on the last two bytes
};

// nunchuk buttons
#define NUNCHUK_C		0x02
#define NUNCHUK_Z		0x01

const u8 nunchuk_button_bitmasks[] =
{
	NUNCHUK_C,
	NUNCHUK_Z,
};

Nunchuk::Nunchuk() : Attachment( "Nunchuk" )
{
	// buttons
	groups.push_back( m_buttons = new Buttons( "Buttons" ) );
	m_buttons->controls.push_back( new ControlGroup::Input( "C" ) );
	m_buttons->controls.push_back( new ControlGroup::Input( "Z" ) );

	// stick
	groups.push_back( m_stick = new AnalogStick( "Stick" ) );

	// tilt
	groups.push_back( m_tilt = new Tilt( "Tilt" ) );

	// swing
	//groups.push_back( m_swing = new Force( "Swing" ) );

	// shake
	groups.push_back( m_shake = new Buttons( "Shake" ) );
	m_shake->controls.push_back( new ControlGroup::Input( "X" ) );
	m_shake->controls.push_back( new ControlGroup::Input( "Y" ) );
	m_shake->controls.push_back( new ControlGroup::Input( "Z" ) );

	// set up register
	// calibration
	memcpy( &reg[0x20], nunchuck_calibration, sizeof(nunchuck_calibration) );
	// id
	memcpy( &reg[0xfa], nunchuck_id, sizeof(nunchuck_id) );

	// this should get set to 0 on disconnect, but it isn't, o well
	memset(m_shake_step, 0, sizeof(m_shake_step));
}

void Nunchuk::GetState( u8* const data, const bool focus )
{
	wm_extension* const ncdata = (wm_extension*)data;

	// stick / not using calibration data for stick, o well
	m_stick->GetState( &ncdata->jx, &ncdata->jy, 0x80, focus ? 127 : 0 );

	// tilt
	EmulateTilt((wm_accel*)&ncdata->ax, m_tilt, (accel_cal*)&reg[0x20], focus);

	if (focus)
	{
		// shake
		EmulateShake(&ncdata->ax, m_shake, m_shake_step);
		// buttons
		m_buttons->GetState( &ncdata->bt, nunchuk_button_bitmasks );
	}
	
	// flip the button bits :/
	ncdata->bt ^= 0x03;
}


}
