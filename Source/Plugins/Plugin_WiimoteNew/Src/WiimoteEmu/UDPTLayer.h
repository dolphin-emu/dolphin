//UDP Wiimote Translation Layer

#ifndef UDPTLAYER_H
#define UDPTLAYER_H

#include "UDPWiimote.h"

namespace UDPTLayer
{
	void GetButtons(UDPWrapper * m , wm_core * butt)
	{
		if (!(m->inst)) return;
		if (!(m->updButt)) return;
		u32 mask=m->inst->getButtons();
		*butt|=(mask&UDPWM_BA)?WIIMOTE_A:0;
		*butt|=(mask&UDPWM_BB)?WIIMOTE_B:0;
		*butt|=(mask&UDPWM_B1)?WIIMOTE_ONE:0;
		*butt|=(mask&UDPWM_B2)?WIIMOTE_TWO:0;
		*butt|=(mask&UDPWM_BP)?WIIMOTE_PLUS:0;
		*butt|=(mask&UDPWM_BM)?WIIMOTE_MINUS:0;
		*butt|=(mask&UDPWM_BH)?WIIMOTE_HOME:0;
		*butt|=(mask&UDPWM_BU)?WIIMOTE_PAD_UP:0;
		*butt|=(mask&UDPWM_BD)?WIIMOTE_PAD_DOWN:0;
		*butt|=(mask&UDPWM_BL)?WIIMOTE_PAD_LEFT:0;
		*butt|=(mask&UDPWM_BR)?WIIMOTE_PAD_RIGHT:0;
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