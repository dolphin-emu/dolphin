#include "Nunchuk.h"

#include "UDPWrapper.h"
#include "UDPWiimote.h"

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
	0xec, 0x41 // checksum on the last two bytes
};

static const u8 nunchuk_button_bitmasks[] =
{
	Nunchuk::BUTTON_C,
	Nunchuk::BUTTON_Z,
};

Nunchuk::Nunchuk(UDPWrapper * wrp) : Attachment( "Nunchuk" ) , udpWrap(wrp)
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
	ncdata->bt = 0;

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
	
	//UDPNunchuk stuff
	if (udpWrap->inst)
	{
		if (udpWrap->updNun)
		{
			u8 mask;
			float x, y;
			udpWrap->inst->getNunchuck(x, y, mask);
			// buttons
			if (mask & UDPWM_NC)
				ncdata->bt &= ~WiimoteEmu::Nunchuk::BUTTON_C;
			if (mask & UDPWM_NZ)
				ncdata->bt &= ~WiimoteEmu::Nunchuk::BUTTON_Z;
			// stick
			if (ncdata->jx == 0x80 && ncdata->jy == 0x80)
			{
				ncdata->jx = u8(0x80 + x*127);
				ncdata->jy = u8(0x80 + y*127);
			}
		}
		if (udpWrap->updNunAccel)
		{
			const accel_cal * const calib = (accel_cal*)&reg[0x20];
			wm_accel * const accel = (wm_accel*)&ncdata->ax;
			float x,y,z;
			udpWrap->inst->getNunchuckAccel(x,y,z);
			accel->x=u8(x*(calib->one_g.x-calib->zero_g.x)+calib->zero_g.x);
			accel->y=u8(y*(calib->one_g.y-calib->zero_g.y)+calib->zero_g.y);
			accel->z=u8(z*(calib->one_g.z-calib->zero_g.z)+calib->zero_g.z);
		}	
	}
	//End UDPNunchuck
}


}
