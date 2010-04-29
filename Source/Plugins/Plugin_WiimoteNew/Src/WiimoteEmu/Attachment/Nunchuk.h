#include "Attachment.h"


namespace WiimoteEmu
{

class Nunchuk : public Attachment
{
public:
	Nunchuk();
	void GetState( u8* const data, const bool focus );

private:
	Tilt*			m_tilt;
	Force*			m_swing;

	Buttons*		m_shake;

	Buttons*		m_buttons;
	AnalogStick*	m_stick;

	unsigned int	m_shake_step[3];
};

}
