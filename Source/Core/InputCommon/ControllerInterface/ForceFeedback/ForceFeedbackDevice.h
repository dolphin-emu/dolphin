// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _FORCEFEEDBACKDEVICE_H_
#define _FORCEFEEDBACKDEVICE_H_

#include "../Device.h"

#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dinput.h>

#include <list>

namespace ciface
{
namespace ForceFeedback
{


class ForceFeedbackDevice : public Core::Device
{
private:
	struct EffectState
	{
		EffectState(LPDIRECTINPUTEFFECT eff) : iface(eff), params(NULL), size(0) {}

		LPDIRECTINPUTEFFECT iface;
		void*               params; // null when force hasn't changed
		u8                  size;   // zero when force should stop
	};

	template <typename P>
	class Force : public Output
	{
	public:
		std::string GetName() const;
		Force(u8 index, EffectState& state);
		void SetState(ControlState state);
	private:
		const u8 m_index;
		EffectState& m_state;
		P params;
	};
	typedef Force<DICONSTANTFORCE>  ForceConstant;
	typedef Force<DIRAMPFORCE>      ForceRamp;
	typedef Force<DIPERIODIC>       ForcePeriodic;

public:
	bool InitForceFeedback(const LPDIRECTINPUTDEVICE8, int cAxes);
	bool UpdateOutput();

	virtual ~ForceFeedbackDevice();
private:
	std::list<EffectState>     m_state_out;
};


}
}

#endif
