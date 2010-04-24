#include "Attachment.h"

namespace WiimoteEmu
{

class Guitar : public Attachment
{
public:
	Guitar();
	void GetState( u8* const data, const bool focus );

private:
	Buttons*		m_buttons;
	Buttons*		m_frets;
	Buttons*		m_strum;
	Triggers*		m_whammy;
	AnalogStick*	m_stick;
};


}
