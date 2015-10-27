// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <string>

#include "InputCommon/ControllerInterface/Device.h"

#ifdef _WIN32
#include <Windows.h>
#include "InputCommon/ControllerInterface/DInput/DInput8.h"
#elif __APPLE__
#include "InputCommon/ControllerInterface/ForceFeedback/OSX/DirectInputAdapter.h"
#endif

namespace ciface
{
namespace ForceFeedback
{


class ForceFeedbackDevice : public Core::Device
{
private:
	template <typename P>
	class Force : public Output
	{
	public:
		Force(const std::string& name, LPDIRECTINPUTEFFECT iface);
		~Force();

		std::string GetName() const override;
		void SetState(ControlState state) override;
		void Update();
		void Stop();
	private:
		const std::string m_name;
		LPDIRECTINPUTEFFECT m_iface;
		P params;
	};
	typedef Force<DICONSTANTFORCE>  ForceConstant;
	typedef Force<DIRAMPFORCE>      ForceRamp;
	typedef Force<DIPERIODIC>       ForcePeriodic;

public:
	bool InitForceFeedback(const LPDIRECTINPUTDEVICE8, int cAxes);
};


}
}
