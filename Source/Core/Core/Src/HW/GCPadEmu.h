#pragma once

#include <ControllerEmu.h>
#include "GCPad.h"

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
	ControlGroup*			m_options;

	const unsigned int		m_index;

};
