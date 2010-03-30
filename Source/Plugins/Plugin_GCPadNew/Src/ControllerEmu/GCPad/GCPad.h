#ifndef _CONEMU_GCPAD_H_
#define _CONEMU_GCPAD_H_

#include "../../ControllerEmu.h"

class GCPad : public ControllerEmu
{
public:

	GCPad( const unsigned int index );
	void GetInput( SPADStatus* const pad );
	void SetOutput( const bool on );
	
	std::string GetName() const;


private:

	Buttons*				m_buttons;
	AnalogStick*			m_main_stick;
	AnalogStick*			m_c_stick;
	Buttons*				m_dpad;
	MixedTriggers*			m_triggers;
	ControlGroup*			m_rumble;

	const unsigned int		m_index;

};


#endif
