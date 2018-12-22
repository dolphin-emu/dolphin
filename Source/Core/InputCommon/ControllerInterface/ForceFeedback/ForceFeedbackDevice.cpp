// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerInterface/ForceFeedback/ForceFeedbackDevice.h"

#include <algorithm>
#include <string>

#include "Common/Thread.h"

namespace ciface
{
namespace ForceFeedback
{
// Template instantiation:
template class ForceFeedbackDevice::TypedForce<DICONSTANTFORCE>;
template class ForceFeedbackDevice::TypedForce<DIRAMPFORCE>;
template class ForceFeedbackDevice::TypedForce<DIPERIODIC>;

struct ForceType
{
  GUID guid;
  const char* name;
};

static const ForceType force_type_names[] = {
    {GUID_ConstantForce, "Constant"},  // DICONSTANTFORCE
    {GUID_RampForce, "Ramp"},          // DIRAMPFORCE
    {GUID_Square, "Square"},           // DIPERIODIC ...
    {GUID_Sine, "Sine"},
    {GUID_Triangle, "Triangle"},
    {GUID_SawtoothUp, "Sawtooth Up"},
    {GUID_SawtoothDown, "Sawtooth Down"},
    //{GUID_Spring, "Spring"},          // DICUSTOMFORCE ... < I think
    //{GUID_Damper, "Damper"},
    //{GUID_Inertia, "Inertia"},
    //{GUID_Friction, "Friction"},
};

void ForceFeedbackDevice::DeInitForceFeedback()
{
  if (!m_run_thread.TestAndClear())
    return;

  SignalUpdateThread();
  m_update_thread.join();
}

void ForceFeedbackDevice::ThreadFunc()
{
  Common::SetCurrentThreadName("ForceFeedback update thread");

  while (m_run_thread.IsSet())
  {
    m_update_event.Wait();

    for (auto output : Outputs())
    {
      auto& force = *static_cast<Force*>(output);
      force.UpdateOutput();
    }
  }

  for (auto output : Outputs())
  {
    auto& force = *static_cast<Force*>(output);
    force.Release();
  }
}

void ForceFeedbackDevice::SignalUpdateThread()
{
  m_update_event.Set();
}

bool ForceFeedbackDevice::InitForceFeedback(const LPDIRECTINPUTDEVICE8 device, int cAxes)
{
  if (cAxes == 0)
    return false;

  // TODO: check for DIDC_FORCEFEEDBACK in devcaps?

  // Temporary for creating the effect:
  DWORD rgdwAxes[2] = {DIJOFS_X, DIJOFS_Y};
  LONG rglDirection[2] = {-200, 0};

  DIEFFECT eff{};
  eff.dwSize = sizeof(DIEFFECT);
  eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
  // Infinite seems to work just fine:
  eff.dwDuration = INFINITE;  // (4 * DI_SECONDS)
  eff.dwSamplePeriod = 0;
  eff.dwGain = DI_FFNOMINALMAX;
  eff.dwTriggerButton = DIEB_NOTRIGGER;
  eff.dwTriggerRepeatInterval = 0;
  eff.cAxes = std::min<DWORD>(1, cAxes);
  eff.rgdwAxes = rgdwAxes;
  eff.rglDirection = rglDirection;

  // Initialize parameters.
  DICONSTANTFORCE diCF{};
  diCF.lMagnitude = DI_FFNOMINALMAX;
  DIRAMPFORCE diRF{};
  DIPERIODIC diPE{};

  // This doesn't seem needed:
  // DIENVELOPE env;
  // eff.lpEnvelope = &env;
  // ZeroMemory(&env, sizeof(env));
  // env.dwSize = sizeof(env);

  for (auto& f : force_type_names)
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
      // All other forces need periodic parameters:
      eff.cbTypeSpecificParams = sizeof(DIPERIODIC);
      eff.lpvTypeSpecificParams = &diPE;
    }

    LPDIRECTINPUTEFFECT pEffect;
    if (SUCCEEDED(device->CreateEffect(f.guid, &eff, &pEffect, nullptr)))
    {
      if (f.guid == GUID_ConstantForce)
        AddOutput(new ForceConstant(this, f.name, pEffect));
      else if (f.guid == GUID_RampForce)
        AddOutput(new ForceRamp(this, f.name, pEffect));
      else
        AddOutput(new ForcePeriodic(this, f.name, pEffect));
    }
  }

  // Disable autocentering:
  if (Outputs().size())
  {
    DIPROPDWORD dipdw;
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = DIPROPAUTOCENTER_OFF;
    device->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph);

    m_run_thread.Set();
    m_update_thread = std::thread(&ForceFeedbackDevice::ThreadFunc, this);
  }

  return true;
}

template <typename P>
void ForceFeedbackDevice::TypedForce<P>::PlayEffect()
{
  DIEFFECT eff{};
  eff.dwSize = sizeof(DIEFFECT);
  eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;

  eff.cbTypeSpecificParams = sizeof(m_params);
  eff.lpvTypeSpecificParams = &m_params;
  m_effect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
}

template <typename P>
void ForceFeedbackDevice::TypedForce<P>::StopEffect()
{
  m_effect->Stop();
}

template <>
bool ForceFeedbackDevice::ForceConstant::UpdateParameters(int magnitude)
{
  const auto old_magnitude = m_params.lMagnitude;

  m_params.lMagnitude = magnitude;

  return old_magnitude != m_params.lMagnitude;
}

template <>
bool ForceFeedbackDevice::ForceRamp::UpdateParameters(int magnitude)
{
  const auto old_magnitude = m_params.lStart;

  m_params.lStart = m_params.lEnd = magnitude;

  return old_magnitude != m_params.lStart;
}

template <>
bool ForceFeedbackDevice::ForcePeriodic::UpdateParameters(int magnitude)
{
  const auto old_magnitude = m_params.dwMagnitude;

  m_params.dwMagnitude = magnitude;
  // Zero is working fine for me:
  // params.dwPeriod = 0;//DWORD(0.05 * DI_SECONDS);

  return old_magnitude != m_params.dwMagnitude;
}

template <typename P>
ForceFeedbackDevice::TypedForce<P>::TypedForce(ForceFeedbackDevice* parent, std::string name,
                                               LPDIRECTINPUTEFFECT effect)
    : Force(parent, std::move(name), effect), m_params{}
{
}

template <typename P>
void ForceFeedbackDevice::TypedForce<P>::UpdateEffect(int magnitude)
{
  if (UpdateParameters(magnitude))
  {
    if (magnitude)
      PlayEffect();
    else
      StopEffect();
  }
}

std::string ForceFeedbackDevice::Force::GetName() const
{
  return m_name;
}

ForceFeedbackDevice::Force::Force(ForceFeedbackDevice* parent, const std::string name,
                                  LPDIRECTINPUTEFFECT effect)
    : m_effect(effect), m_parent(*parent), m_name(std::move(name)), m_desired_magnitude()
{
}

void ForceFeedbackDevice::Force::SetState(ControlState state)
{
  const auto new_val = int(DI_FFNOMINALMAX * state);

  if (m_desired_magnitude.exchange(new_val) != new_val)
    m_parent.SignalUpdateThread();
}

void ForceFeedbackDevice::Force::UpdateOutput()
{
  UpdateEffect(m_desired_magnitude);
}

void ForceFeedbackDevice::Force::Release()
{
  // This isn't in the destructor because it should happen before the device is released.
  m_effect->Stop();
  m_effect->Unload();
  m_effect->Release();
}

}  // namespace ForceFeedback
}  // namespace ciface
