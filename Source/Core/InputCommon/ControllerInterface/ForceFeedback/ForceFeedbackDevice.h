// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <string>
#include <thread>

#include <Windows.h>

#include "Common/Event.h"
#include "Common/Flag.h"

#include "InputCommon/ControllerInterface/CoreDevice.h"
#include "InputCommon/ControllerInterface/DInput/DInput8.h"

namespace ciface::ForceFeedback
{
class ForceFeedbackDevice : public Core::Device
{
public:
  bool InitForceFeedback(const LPDIRECTINPUTDEVICE8, int axis_count);
  void DeInitForceFeedback();

private:
  void ThreadFunc();

  class Force : public Output
  {
  public:
    Force(ForceFeedbackDevice* parent, const char* name, LPDIRECTINPUTEFFECT effect);

    void UpdateOutput();
    void Release();

    void SetState(ControlState state) override;
    std::string GetName() const override;

  protected:
    const LPDIRECTINPUTEFFECT m_effect;

  private:
    virtual void UpdateEffect(int magnitude) = 0;

    ForceFeedbackDevice& m_parent;
    const char* const m_name;
    std::atomic<int> m_desired_magnitude;
  };

  template <typename P>
  class TypedForce : public Force
  {
  public:
    TypedForce(ForceFeedbackDevice* parent, const char* name, LPDIRECTINPUTEFFECT effect,
               const P& params);

  private:
    void UpdateEffect(int magnitude) override;

    // Returns true if parameters changed.
    bool UpdateParameters(int magnitude);

    void PlayEffect();
    void StopEffect();

    P m_params = {};
  };

  void SignalUpdateThread();

  typedef TypedForce<DICONSTANTFORCE> ForceConstant;
  typedef TypedForce<DIRAMPFORCE> ForceRamp;
  typedef TypedForce<DIPERIODIC> ForcePeriodic;

  std::thread m_update_thread;
  Common::Event m_update_event;
  Common::Flag m_run_thread;
};
}  // namespace ciface::ForceFeedback
