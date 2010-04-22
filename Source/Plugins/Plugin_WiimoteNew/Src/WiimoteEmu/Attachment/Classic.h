#include "Attachment.h"

namespace WiimoteEmu
{

class Classic : public Attachment
{
public:
	Classic();
	void GetState( u8* const data, const bool focus );

private:
	Buttons*		m_buttons;
	Buttons*		m_shake;
	MixedTriggers*	m_triggers;
	Buttons*		m_dpad;
	AnalogStick*	m_left_stick;
	AnalogStick*	m_right_stick;
};


}
