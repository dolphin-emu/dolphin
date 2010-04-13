#include "Nunchuk.h"


namespace WiimoteEmu
{

static const u8 nunchuck_id[]	= { 0x00, 0x00, 0xa4, 0x20, 0x00, 0x00 };
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

	// force
	//groups.push_back( m_tilt = new Tilt( "Tilt" ) );
	groups.push_back( m_tilt = new Tilt( "Pitch and Roll" ) );
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
}

void Nunchuk::GetState( u8* const data )
{
	wm_extension* const ncdata = (wm_extension*)data;

	// stick / not using calibration data for stick, o well
	m_stick->GetState( &ncdata->jx, &ncdata->jy, 0x80, 127 );

	// tilt
	float x, y;
	m_tilt->GetState( &x, &y, 0, (PI / 2) ); // 90 degrees

	// this isn't doing anything with those low bits in the calib data, o well

	const accel_cal* const cal = (accel_cal*)&reg[0x20];
	const u8* const zero_g = &cal->zero_g.x;
	u8 one_g[3];
	for ( unsigned int i=0; i<3; ++i )
		one_g[i] = (&cal->one_g.x)[i] - zero_g[i];

	// this math should be good enough :P
	ncdata->az = u8(sin( (PI / 2) - std::max( abs(x), abs(y) ) ) * one_g[2] + zero_g[2]);
	ncdata->ax = u8(sin(x) * -one_g[0] + zero_g[0]);
	ncdata->ay = u8(sin(y) * one_g[1] + zero_g[1]);

	// shake
	const unsigned int btns[] = { 0x01, 0x02, 0x04 };
	unsigned int shake = 0;
	m_shake->GetState( &shake, btns );
	static unsigned int shake_step = 0;
	if (shake)
	{
		shake_step = (shake_step + 1) % sizeof(shake_data);
		for ( unsigned int i=0; i<3; ++i )
			if ( shake & (1 << i) )
				(&ncdata->ax)[i] = shake_data[shake_step];
	}
	else
		shake_step = 0;

	// buttons
	m_buttons->GetState( &ncdata->bt, nunchuk_button_bitmasks );
	
	// flip the button bits :/
	ncdata->bt ^= 0x3;
}


}
