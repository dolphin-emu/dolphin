// Copyright (C) 2010 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _CONEMU_GCPAD_H_
#define _CONEMU_GCPAD_H_

#include <string>

#include "ControllerEmu.h"

class GCPad : public ControllerEmu
{
public:

	GCPad(const unsigned int index);
	void GetInput(SPADStatus* const pad);
	void SetOutput(const bool on);

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
