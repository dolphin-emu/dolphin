// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>
#include <algorithm>
#include "InputCommon/ControllerInterface/ForceFeedback/ForceFeedbackDevice.h"
#include "InputCommon/ControllerInterface/DInput/DInput.h"

namespace ciface
{
namespace ForceFeedback
{

// effects will stop after this (when you pause a game and such)
static const DWORD EFFECT_DURATION = 4 * DI_SECONDS;
// used for periodic effect type
static const DWORD EFFECT_PERIOD = DI_SECONDS / 4;

struct ForceType
{
	GUID guid;
	const std::string name;
	DWORD effect_type;
};

// TODO: Should we use EnumEffects instead?
// Or is it as questionable as EnumObjects?
static const ForceType force_type_names[] =
{
	// DIEFT_CONSTANTFORCE
	{ GUID_ConstantForce, "Constant", DIEFT_CONSTANTFORCE },
	// DIEFT_RAMPFORCE
	{ GUID_RampForce, "Ramp", DIEFT_RAMPFORCE },
	// DIEFT_PERIODIC ...
	{ GUID_Square, "Square", DIEFT_PERIODIC },
	{ GUID_Sine, "Sine", DIEFT_PERIODIC },
	{ GUID_Triangle, "Triangle", DIEFT_PERIODIC },
	{ GUID_SawtoothUp, "Sawtooth Up", DIEFT_PERIODIC },
	{ GUID_SawtoothDown, "Sawtooth Down", DIEFT_PERIODIC },
	// DIEFT_CONDITION ...
	{ GUID_Spring, "Spring", DIEFT_CONDITION },
	{ GUID_Damper, "Damper", DIEFT_CONDITION },
	{ GUID_Inertia, "Inertia", DIEFT_CONDITION },
	{ GUID_Friction, "Friction", DIEFT_CONDITION },
};

ForceFeedbackDevice::ForceFeedbackDevice(const LPDIRECTINPUTDEVICE8 device)
{
	// disable autocentering (needs to happen before "Acquire" with my sidewinder joystick)
	// This should probably be optional in some way..
	DInput::SetDeviceProperty(device, DIPROP_AUTOCENTER, DIPROPAUTOCENTER_OFF);
}

void ForceFeedbackDevice::InitForceFeedback(const LPDIRECTINPUTDEVICE8 device)
{
	DICONSTANTFORCE diCF = {};
	diCF.lMagnitude = DI_FFNOMINALMAX;

	// This is probably not sane
	DIRAMPFORCE diRF = {};
	diRF.lStart = 0;
	diRF.lEnd = DI_FFNOMINALMAX;

	DIPERIODIC diPE = {};
	diPE.dwMagnitude = DI_FFNOMINALMAX;
	diPE.dwPeriod = EFFECT_PERIOD;

	DICONDITION diCO = {};
	diCO.lPositiveCoefficient = DI_FFNOMINALMAX;
	diCO.lNegativeCoefficient = DI_FFNOMINALMAX;
	diCO.dwPositiveSaturation = DI_FFNOMINALMAX;
	diCO.dwNegativeSaturation = DI_FFNOMINALMAX;
	diCO.lDeadBand = DI_FFNOMINALMAX * 5 / 100;

	for (const ForceType& force : force_type_names)
	{
		DIEFFECT eff = {};
		eff.dwSize = sizeof(eff);
		eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
		eff.dwDuration = EFFECT_PERIOD;
		eff.dwSamplePeriod = 0;
		eff.dwGain = DI_FFNOMINALMAX;
		eff.dwTriggerButton = DIEB_NOTRIGGER;
		eff.dwTriggerRepeatInterval = 0;
		eff.lpEnvelope = nullptr;
		eff.cbTypeSpecificParams = 0;
		eff.lpvTypeSpecificParams = nullptr;
		eff.dwStartDelay = 0;

		if (force.effect_type == DIEFT_CONSTANTFORCE)
		{
			eff.cbTypeSpecificParams = sizeof(diCF);
			eff.lpvTypeSpecificParams = &diCF;
		}
		else if (force.effect_type == DIEFT_RAMPFORCE)
		{
			eff.cbTypeSpecificParams = sizeof(diRF);
			eff.lpvTypeSpecificParams = &diRF;
		}
		else if (force.effect_type == DIEFT_PERIODIC)
		{
			eff.cbTypeSpecificParams = sizeof(diPE);
			eff.lpvTypeSpecificParams = &diPE;
		}
		else if (force.effect_type == DIEFT_CONDITION)
		{
			eff.cbTypeSpecificParams = sizeof(diCO);
			eff.lpvTypeSpecificParams = &diCO;
		}

		std::vector<DWORD> valid_ff_axes;

		// There are 8 axes in the DIJOYSTATE data format we are using.
		// Let's try to CreateEffect with all of them. (EnumObjects is a joke)
		// This might give us a vector of axes we can use.
		for (unsigned int offset = 0; offset != 8; ++offset)
		{
			DWORD rgdwAxes = offset * sizeof(LONG);
			LONG rglDirection = 1;

			eff.cAxes = 1;
			eff.rgdwAxes = &rgdwAxes;
			eff.rglDirection = &rglDirection;

			LPDIRECTINPUTEFFECT testeffect;
			if (SUCCEEDED(device->CreateEffect(force.guid, nullptr, &testeffect, nullptr)))
			{
				// DIERR_INCOMPLETEEFFECT seems to mean we can use this axis (vs. DIERR_INVALIDPARAM)
				if (DIERR_INCOMPLETEEFFECT == testeffect->SetParameters(&eff, DIEP_AXES | DIEP_TYPESPECIFICPARAMS))
					valid_ff_axes.push_back(offset * sizeof(LONG));

				testeffect->Release();
			}			
		}

		std::vector<LONG> rglDirection(valid_ff_axes.size());

		eff.cAxes = (DWORD)valid_ff_axes.size();
		eff.rgdwAxes = valid_ff_axes.data();
		eff.rglDirection = rglDirection.data();

		LPDIRECTINPUTEFFECT effect_iface;
		if (SUCCEEDED(device->CreateEffect(force.guid, nullptr, &effect_iface, nullptr)))
		{
			// Don't "download" now. It's randomly crashy for some reason..
			if (SUCCEEDED(effect_iface->SetParameters(&eff, DIEP_ALLPARAMS | DIEP_NODOWNLOAD)))
			{
				m_effect_states.emplace_back(effect_iface, valid_ff_axes.size());

				// Now create an "Output" for each axis.
				// The state of each of these outputs will adjust the direction and magnitude of the effect.
				for (size_t i = 0; i != valid_ff_axes.size(); ++i)
				{
					std::string const force_name = DInput::DIJOYSTATE_AxisName(valid_ff_axes[i] / sizeof(LONG)) + " " + force.name;
					AddOutput(new Force(force_name, m_effect_states.back(), i));
				}
			}
			else
				effect_iface->Release();
		}
	}
}

bool ForceFeedbackDevice::UpdateOutput()
{
	size_t ok_count = 0;

	DIEFFECT eff = {};
	eff.dwSize = sizeof(eff);

	for (EffectState& effect : m_effect_states)
	{
		ok_count += effect.Update();
	}

	return (m_effect_states.size() == ok_count);
}

ForceFeedbackDevice::EffectState::EffectState(LPDIRECTINPUTEFFECT iface, std::size_t axis_count)
	: m_iface(iface)
	, m_axis_directions(axis_count)
	, m_dirty()
{}

ForceFeedbackDevice::EffectState::~EffectState()
{
	// Implicit stop
	m_iface->Unload();
	// Is it ok to release this after the device is released like we are doing?
	m_iface->Release();
}

ForceFeedbackDevice::Force::Force(const std::string& name, EffectState& effect_state_ref, std::size_t axis_index)
	: m_name(name)
	, m_effect_state_ref(effect_state_ref)
	, m_axis_index(axis_index)
{}

// Moved to a separate function to avoid SEH causing "error C2712".
bool StartEffect(LPDIRECTINPUTEFFECT iface)
{
	__try
	{
		// For some reason the "Download" operation crashes on occasion.
		// The only "solution" I've found is to catch the access violation.
		return SUCCEEDED(iface->Start(EFFECT_DURATION / EFFECT_PERIOD, 0));
	}
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		return false;
	}

	return true;
}

bool ForceFeedbackDevice::EffectState::Update()
{
	DIEFFECT eff = {};
	eff.dwSize = sizeof(eff);

	if (m_dirty)
	{
		m_dirty = false;

		DWORD new_gain = 0;
		for (LONG force : m_axis_directions)
			new_gain = std::max(new_gain, (DWORD)std::abs(force));

		if (new_gain != 0)
		{
			eff.dwFlags = DIEFF_CARTESIAN;
			eff.cAxes = (DWORD)m_axis_directions.size();
			eff.rglDirection = m_axis_directions.data();
			eff.dwGain = new_gain;
			//eff.dwDuration = EFFECT_DURATION;

			m_iface->SetParameters(&eff, DIEP_DIRECTION | DIEP_GAIN | /*DIEP_DURATION | */DIEP_NODOWNLOAD);
			return StartEffect(m_iface);
		}
		else
		{
			// An alternative/supplement to Stop/Unload is setting gain and/or duration to 0.
			//eff.dwGain = 0;
			//eff.dwDuration = 0;
			//m_iface->SetParameters(&eff, DIEP_GAIN | DIEP_DURATION | DIEP_START);
			//m_iface->Stop();
			// TODO: Is Unloading and downloading the effect every time it restarts sane?
			return SUCCEEDED(m_iface->Unload());
		}
	}

	return true;
}

void ForceFeedbackDevice::EffectState::SetAxisForce(std::size_t axis_offset, LONG magnitude)
{
	if (magnitude != m_axis_directions[axis_offset])
	{
		m_dirty = true;
		m_axis_directions[axis_offset] = magnitude;
	}
}

void ForceFeedbackDevice::Force::SetState(const ControlState state)
{
	const LONG new_gain = LONG(DI_FFNOMINALMAX * state);
	m_effect_state_ref.SetAxisForce(m_axis_index, -new_gain);
}

std::string ForceFeedbackDevice::Force::GetName() const
{
	return m_name;
}

}
}
