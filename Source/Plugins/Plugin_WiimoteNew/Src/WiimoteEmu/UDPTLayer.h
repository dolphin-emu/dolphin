//UDP Wiimote Translation Layer

#ifndef UDPTLAYER_H
#define UDPTLAYER_H

#include "UDPWiimote.h"
#include "WiimoteEmu.h"

using WiimoteEmu::Wiimote;

namespace UDPTLayer
{
	void GetButtons(UDPWrapper * m , wm_core * butt)
	{
		if (!(m->inst)) return;
		if (!(m->updButt)) return;
		u32 mask=m->inst->getButtons();
		*butt|=(mask&UDPWM_BA)?Wiimote::BUTTON_A:0;
		*butt|=(mask&UDPWM_BB)?Wiimote::BUTTON_B:0;
		*butt|=(mask&UDPWM_B1)?Wiimote::BUTTON_ONE:0;
		*butt|=(mask&UDPWM_B2)?Wiimote::BUTTON_TWO:0;
		*butt|=(mask&UDPWM_BP)?Wiimote::BUTTON_PLUS:0;
		*butt|=(mask&UDPWM_BM)?Wiimote::BUTTON_MINUS:0;
		*butt|=(mask&UDPWM_BH)?Wiimote::BUTTON_HOME:0;
		*butt|=(mask&UDPWM_BU)?Wiimote::PAD_UP:0;
		*butt|=(mask&UDPWM_BD)?Wiimote::PAD_DOWN:0;
		*butt|=(mask&UDPWM_BL)?Wiimote::PAD_LEFT:0;
		*butt|=(mask&UDPWM_BR)?Wiimote::PAD_RIGHT:0;
	}

	void GetAcceleration(UDPWrapper * m , wm_accel * data, accel_cal * calib)
	{
		if (!(m->inst)) return;
		if (!(m->updAccel)) return;
		float x,y,z;
		m->inst->getAccel(x,y,z);
		data->x=u8(x*(calib->one_g.x-calib->zero_g.x)+calib->zero_g.x);
		data->y=u8(y*(calib->one_g.y-calib->zero_g.y)+calib->zero_g.y);
		data->z=u8(z*(calib->one_g.z-calib->zero_g.z)+calib->zero_g.z);
	}

	void GetIR( UDPWrapper * m, float * x,  float * y,  float * z)
	{
		if (!(m->inst)) return;
		if (!(m->updIR)) return;
		if ((*x>-1)&&(*x<1)&&(*y>-1)&&(*y<1)) return; //the recieved values are used ONLY when the normal pointer is offscreen
		float _x,_y;
		m->inst->getIR(_x,_y);
		*x=_x*2-1;
		*y=-(_y*2-1);
		*z=0;
	}
	
}

#endif
