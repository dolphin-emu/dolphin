// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _CONEMU_GCPAD_H_
#define _CONEMU_GCPAD_H_

#include <string>

#include "ControllerEmu.h"

class GCPad : public ControllerEmu
{
public:

	GCPad(const unsigned int index);
	void GetInput(SPADStatus* const pad);
	void SetOutput(const u8 on);
	void SetMotor(const u8 on);

	bool GetMicButton() const;
	
	std::string GetName() const;

	void LoadDefaults(const ControllerInterface& ciface);

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

#endif
