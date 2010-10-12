#ifndef NUNCHUCK_H
#define NUNCHUCK_H

#include "Attachment.h"

class UDPWrapper;

namespace WiimoteEmu
{

class Nunchuk : public Attachment
{
public:

#ifdef USE_UDP_WIIMOTE
	Nunchuk(UDPWrapper * wrp);
#else
	Nunchuk();
#endif

	virtual void GetState( u8* const data, const bool focus );

	enum
	{
		BUTTON_C = 0x02,
		BUTTON_Z = 0x01,
	};

private:
	Tilt*			m_tilt;
	Force*			m_swing;

	Buttons*		m_shake;

	Buttons*		m_buttons;
	AnalogStick*	m_stick;

	unsigned int	m_shake_step[3];
	
#ifdef USE_UDP_WIIMOTE
	UDPWrapper* const m_udpWrap;
#endif
};

}

#endif
