// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <string>
#include "Common/Thread.h"
#include "InputCommon/ControllerInterface/ForceFeedback/ForceFeedbackDevice.h"

namespace ciface
{
namespace ForceFeedback
{

// template instantiation
template class ForceFeedbackDevice::Force<DICONSTANTFORCE>;
template class ForceFeedbackDevice::Force<DIRAMPFORCE>;
template class ForceFeedbackDevice::Force<DIPERIODIC>;

struct ForceType
{
	GUID guid;
	const std::string name;
};

static const ForceType force_type_names[] =
{
	{GUID_ConstantForce, "Constant"}, // DICONSTANTFORCE
	{GUID_RampForce, "Ramp"},         // DIRAMPFORCE
	{GUID_Square, "Square"},          // DIPERIODIC ...
	{GUID_Sine, "Sine"},
	{GUID_Triangle, "Triangle"},
	{GUID_SawtoothUp, "Sawtooth Up"},
	{GUID_SawtoothDown, "Sawtooth Down"},
	//{GUID_Spring, "Spring"},          // DICUSTOMFORCE ... < I think
	//{GUID_Damper, "Damper"},
	//{GUID_Inertia, "Inertia"},
	//{GUID_Friction, "Friction"},
};

ForceFeedbackDevice::~ForceFeedbackDevice()
{
	// release the ff effect iface's
	for (EffectState& state : m_state_out)
	{
		state.iface->Stop();
		state.iface->Unload();
		state.iface->Release();
	}
}

bool ForceFeedbackDevice::InitForceFeedback(const LPDIRECTINPUTDEVICE8 device, int cAxes)
{
	if (cAxes == 0)
		return false;

	// TODO: check for DIDC_FORCEFEEDBACK in devcaps?

	// temporary
	DWORD rgdwAxes[2] = {DIJOFS_X, DIJOFS_Y};
	LONG rglDirection[2] = {-200, 0};

	DIEFFECT eff;
	memset(&eff, 0, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
	eff.dwDuration = INFINITE; // (4 * DI_SECONDS)
	eff.dwSamplePeriod = 0;
	eff.dwGain = DI_FFNOMINALMAX;
	eff.dwTriggerButton = DIEB_NOTRIGGER;
	eff.dwTriggerRepeatInterval = 0;
	eff.cAxes = std::min<DWORD>(1, cAxes);
	eff.rgdwAxes = rgdwAxes;
	eff.rglDirection = rglDirection;

	// initialize parameters
	DICONSTANTFORCE diCF = { -10000 };
	diCF.lMagnitude = DI_FFNOMINALMAX;
	DIRAMPFORCE diRF = { 0 };
	DIPERIODIC diPE = { 0 };

	// doesn't seem needed
	//DIENVELOPE env;
	//eff.lpEnvelope = &env;
	//ZeroMemory(&env, sizeof(env));
	//env.dwSize = sizeof(env);

	for (const ForceType& f : force_type_names)
	{
		if (f.guid == GUID_ConstantForce)
		{
			eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
			eff.lpvTypeSpecificParams = &diCF;
		}
		else if (f.guid == GUID_RampForce)
		{
			eff.cbTypeSpecificParams = sizeof(DIRAMPFORCE);
			eff.lpvTypeSpecificParams = &diRF;
		}
		else
		{
			// all other forces need periodic parameters
			eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
			eff.lpvTypeSpecificParams = &diPE;
		}

		LPDIRECTINPUTEFFECT pEffect;
		if (SUCCEEDED(device->CreateEffect(f.guid, &eff, &pEffect, nullptr)))
		{
			m_state_out.push_back(EffectState(pEffect));

			if (f.guid == GUID_ConstantForce)
				AddOutput(new ForceConstant(f.name, m_state_out.back()));
			else if (f.guid == GUID_RampForce)
				AddOutput(new ForceRamp(f.name, m_state_out.back()));
			else
				AddOutput(new ForcePeriodic(f.name, m_state_out.back()));
		}
	}

	// disable autocentering
	if (Outputs().size())
	{
		DIPROPDWORD dipdw;
		dipdw.diph.dwSize = sizeof( DIPROPDWORD );
		dipdw.diph.dwHeaderSize = sizeof( DIPROPHEADER );
		dipdw.diph.dwObj = 0;
		dipdw.diph.dwHow = DIPH_DEVICE;
		dipdw.dwData = DIPROPAUTOCENTER_OFF;
		device->SetProperty( DIPROP_AUTOCENTER, &dipdw.diph );
	}

	return true;
}

bool ForceFeedbackDevice::UpdateOutput()
{
	size_t ok_count = 0;

	DIEFFECT eff;
	memset(&eff, 0, sizeof(eff));
	eff.dwSize = sizeof(DIEFFECT);
	eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;

	for (EffectState& state : m_state_out)
	{
		if (state.params)
		{
			if (state.size)
			{
				eff.cbTypeSpecificParams = state.size;
				eff.lpvTypeSpecificParams = state.params;
				// set params and start effect
				ok_count += SUCCEEDED(state.iface->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START));
			}
			else
			{
				ok_count += SUCCEEDED(state.iface->Stop());
			}

			state.params = nullptr;
		}
		else
		{
			++ok_count;
		}
	}

	return (m_state_out.size() == ok_count);
}

template<>
void ForceFeedbackDevice::ForceConstant::SetState(const ControlState state)
{
	const LONG new_val = LONG(10000 * state);

	LONG &val = params.lMagnitude;
	if (val != new_val)
	{
		val = new_val;
		m_state.params = &params; // tells UpdateOutput the state has changed

		// tells UpdateOutput to either start or stop the force
		m_state.size = new_val ? sizeof(params) : 0;
	}
}

template<>
void ForceFeedbackDevice::ForceRamp::SetState(const ControlState state)
{
	const LONG new_val = LONG(10000 * state);

	if (params.lStart != new_val)
	{
		params.lStart = params.lEnd = new_val;
		m_state.params = &params; // tells UpdateOutput the state has changed

		// tells UpdateOutput to either start or stop the force
		m_state.size = new_val ? sizeof(params) : 0;
	}
}

template<>
void ForceFeedbackDevice::ForcePeriodic::SetState(const ControlState state)
{
	const DWORD new_val = DWORD(10000 * state);

	DWORD &val = params.dwMagnitude;
	if (val != new_val)
	{
		val = new_val;
		//params.dwPeriod = 0;//DWORD(0.05 * DI_SECONDS); // zero is working fine for me

		m_state.params = &params; // tells UpdateOutput the state has changed

		// tells UpdateOutput to either start or stop the force
		m_state.size = new_val ? sizeof(params) : 0;
	}
}

template <typename P>
ForceFeedbackDevice::Force<P>::Force(const std::string& name, EffectState& state)
: m_name(name), m_state(state)
{
	memset(&params, 0, sizeof(params));
}

template <typename P>
std::string ForceFeedbackDevice::Force<P>::GetName() const
{
	return m_name;
}

}
}
