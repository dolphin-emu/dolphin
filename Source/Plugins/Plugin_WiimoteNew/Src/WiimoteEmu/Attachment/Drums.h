#include "Attachment.h"

namespace WiimoteEmu
{

class Drums : public Attachment
{
public:
	Drums();
	void GetState( u8* const data, const bool focus );

private:
	Buttons*		m_buttons;
	Buttons*		m_pads;
	AnalogStick*	m_stick;
};


}
