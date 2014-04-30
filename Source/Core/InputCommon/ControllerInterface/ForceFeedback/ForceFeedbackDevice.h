// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <string>

#include "InputCommon/ControllerInterface/Device.h"

#ifdef _WIN32
#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <dinput.h>
#elif __APPLE__
#include "InputCommon/ControllerInterface/ForceFeedback/OSX/DirectInputAdapter.h"
#endif

namespace ciface
{
namespace ForceFeedback
{


class ForceFeedbackDevice : public Core::Device
{
public:
	void InitForceFeedback(const LPDIRECTINPUTDEVICE8);
	bool UpdateOutput();

	ForceFeedbackDevice(const LPDIRECTINPUTDEVICE8 device);

private:
	class EffectState : NonCopyable
	{
	public:
		EffectState(LPDIRECTINPUTEFFECT iface, std::size_t axis_count);
		~EffectState();

		bool Update();
		void SetAxisForce(std::size_t axis_offset, LONG magnitude);

	private:
		LPDIRECTINPUTEFFECT const m_iface;
		std::vector<LONG> m_axis_directions;
		bool m_dirty;
	};

	class Force : public Output
	{
	public:
		std::string GetName() const;
		Force(const std::string& name, EffectState& effect_state_ref, std::size_t axis_index);

		void SetState(ControlState state);

	private:
		const std::string m_name;
		EffectState& m_effect_state_ref;
		std::size_t const m_axis_index;
	};

	std::list<EffectState> m_effect_states;
};


}
}
