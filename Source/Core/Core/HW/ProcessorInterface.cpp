// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HW/ProcessorInterface.h"

#include <cstdio>
#include <memory>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/STM/STM.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/Fifo.h"

namespace ProcessorInterface
{
constexpr u32 FLIPPER_REV_A [[maybe_unused]] = 0x046500B0;
constexpr u32 FLIPPER_REV_B [[maybe_unused]] = 0x146500B1;
constexpr u32 FLIPPER_REV_C = 0x246500B1;

ProcessorInterfaceManager::ProcessorInterfaceManager(Core::System& system) : m_system(system)
{
}

ProcessorInterfaceManager::~ProcessorInterfaceManager() = default;

void ProcessorInterfaceManager::DoState(PointerWrap& p)
{
  p.Do(m_interrupt_mask);
  p.Do(m_interrupt_cause);
  p.Do(m_fifo_cpu_base);
  p.Do(m_fifo_cpu_end);
  p.Do(m_fifo_cpu_write_pointer);
  p.Do(m_reset_code);
}

void ProcessorInterfaceManager::Init()
{
  m_interrupt_mask = 0;
  m_interrupt_cause = 0;

  m_fifo_cpu_base = 0;
  m_fifo_cpu_end = 0;
  m_fifo_cpu_write_pointer = 0;

  m_reset_code = 0;  // Cold reset
  m_interrupt_cause = INT_CAUSE_RST_BUTTON | INT_CAUSE_VI;

  auto& core_timing = m_system.GetCoreTiming();
  m_event_type_toggle_reset_button =
      core_timing.RegisterEvent("ToggleResetButton", ToggleResetButtonCallback);
  m_event_type_ios_notify_reset_button =
      core_timing.RegisterEvent("IOSNotifyResetButton", IOSNotifyResetButtonCallback);
  m_event_type_ios_notify_power_button =
      core_timing.RegisterEvent("IOSNotifyPowerButton", IOSNotifyPowerButtonCallback);
}

void ProcessorInterfaceManager::RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  mmio->Register(base | PI_INTERRUPT_CAUSE, MMIO::DirectRead<u32>(&m_interrupt_cause),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& processor_interface = system.GetProcessorInterface();
                   processor_interface.m_interrupt_cause &= ~val;
                   processor_interface.UpdateException();
                 }));

  mmio->Register(base | PI_INTERRUPT_MASK, MMIO::DirectRead<u32>(&m_interrupt_mask),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& processor_interface = system.GetProcessorInterface();
                   processor_interface.m_interrupt_mask = val;
                   processor_interface.UpdateException();
                 }));

  mmio->Register(base | PI_FIFO_BASE, MMIO::DirectRead<u32>(&m_fifo_cpu_base),
                 MMIO::DirectWrite<u32>(&m_fifo_cpu_base, 0xFFFFFFE0));

  mmio->Register(base | PI_FIFO_END, MMIO::DirectRead<u32>(&m_fifo_cpu_end),
                 MMIO::DirectWrite<u32>(&m_fifo_cpu_end, 0xFFFFFFE0));

  mmio->Register(base | PI_FIFO_WPTR, MMIO::DirectRead<u32>(&m_fifo_cpu_write_pointer),
                 MMIO::DirectWrite<u32>(&m_fifo_cpu_write_pointer, 0xFFFFFFE0));

  mmio->Register(base | PI_FIFO_RESET, MMIO::InvalidRead<u32>(),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   // Used by GXAbortFrame
                   INFO_LOG_FMT(PROCESSORINTERFACE, "Wrote PI_FIFO_RESET: {:08x}", val);
                   if ((val & 1) != 0)
                   {
                     system.GetGPFifo().ResetGatherPipe();

                     // Call Fifo::ResetVideoBuffer() from the video thread. Since that function
                     // resets various pointers used by the video thread, we can't call it directly
                     // from the CPU thread, so queue a task to do it instead. In single-core mode,
                     // AsyncRequests is in passthrough mode, so this will be safely and immediately
                     // called on the CPU thread.

                     // NOTE: GPFifo::ResetGatherPipe() only affects
                     // CPU state, so we can call it directly

                     AsyncRequests::Event ev = {};
                     ev.type = AsyncRequests::Event::FIFO_RESET;
                     AsyncRequests::GetInstance()->PushEvent(ev);
                   }
                 }));

  mmio->Register(base | PI_RESET_CODE, MMIO::ComplexRead<u32>([](Core::System& system, u32) {
                   auto& processor_interface = system.GetProcessorInterface();
                   DEBUG_LOG_FMT(PROCESSORINTERFACE, "Read PI_RESET_CODE: {:08x}",
                                 processor_interface.m_reset_code);
                   return processor_interface.m_reset_code;
                 }),
                 MMIO::ComplexWrite<u32>([](Core::System& system, u32, u32 val) {
                   auto& processor_interface = system.GetProcessorInterface();
                   processor_interface.m_reset_code = val;
                   INFO_LOG_FMT(PROCESSORINTERFACE, "Wrote PI_RESET_CODE: {:08x}",
                                processor_interface.m_reset_code);
                   if (!system.IsWii() && (~processor_interface.m_reset_code & 0x4))
                   {
                     system.GetDVDInterface().ResetDrive(true);
                   }
                 }));

  mmio->Register(base | PI_FLIPPER_REV, MMIO::Constant<u32>(FLIPPER_REV_C),
                 MMIO::InvalidWrite<u32>());

  // 16 bit reads are based on 32 bit reads.
  for (u32 i = 0; i < 0x1000; i += 4)
  {
    mmio->Register(base | i, MMIO::ReadToLarger<u16>(mmio, base | i, 16),
                   MMIO::InvalidWrite<u16>());
    mmio->Register(base | (i + 2), MMIO::ReadToLarger<u16>(mmio, base | i, 0),
                   MMIO::InvalidWrite<u16>());
  }
}

void ProcessorInterfaceManager::UpdateException()
{
  auto& ppc_state = m_system.GetPPCState();
  if ((m_interrupt_cause & m_interrupt_mask) != 0)
    ppc_state.Exceptions |= EXCEPTION_EXTERNAL_INT;
  else
    ppc_state.Exceptions &= ~EXCEPTION_EXTERNAL_INT;
}

static const char* Debug_GetInterruptName(u32 cause_mask)
{
  switch (cause_mask)
  {
  case INT_CAUSE_PI:
    return "INT_CAUSE_PI";
  case INT_CAUSE_DI:
    return "INT_CAUSE_DI";
  case INT_CAUSE_RSW:
    return "INT_CAUSE_RSW";
  case INT_CAUSE_SI:
    return "INT_CAUSE_SI";
  case INT_CAUSE_EXI:
    return "INT_CAUSE_EXI";
  case INT_CAUSE_AI:
    return "INT_CAUSE_AI";
  case INT_CAUSE_DSP:
    return "INT_CAUSE_DSP";
  case INT_CAUSE_MEMORY:
    return "INT_CAUSE_MEMORY";
  case INT_CAUSE_VI:
    return "INT_CAUSE_VI";
  case INT_CAUSE_PE_TOKEN:
    return "INT_CAUSE_PE_TOKEN";
  case INT_CAUSE_PE_FINISH:
    return "INT_CAUSE_PE_FINISH";
  case INT_CAUSE_CP:
    return "INT_CAUSE_CP";
  case INT_CAUSE_DEBUG:
    return "INT_CAUSE_DEBUG";
  case INT_CAUSE_WII_IPC:
    return "INT_CAUSE_WII_IPC";
  case INT_CAUSE_HSP:
    return "INT_CAUSE_HSP";
  case INT_CAUSE_RST_BUTTON:
    return "INT_CAUSE_RST_BUTTON";
  default:
    return "!!! ERROR-unknown Interrupt !!!";
  }
}

void ProcessorInterfaceManager::SetInterrupt(u32 cause_mask, bool set)
{
  DEBUG_ASSERT_MSG(POWERPC, Core::IsCPUThread(), "SetInterrupt from wrong thread");

  if (set && !(m_interrupt_cause & cause_mask))
  {
    DEBUG_LOG_FMT(PROCESSORINTERFACE, "Setting Interrupt {} (set)",
                  Debug_GetInterruptName(cause_mask));
  }

  if (!set && (m_interrupt_cause & cause_mask))
  {
    DEBUG_LOG_FMT(PROCESSORINTERFACE, "Setting Interrupt {} (clear)",
                  Debug_GetInterruptName(cause_mask));
  }

  if (set)
    m_interrupt_cause |= cause_mask;
  else
    m_interrupt_cause &= ~cause_mask;  // is there any reason to have this possibility?
  // F|RES: i think the hw devices reset the interrupt in the PI to 0
  // if the interrupt cause is eliminated. that isn't done by software (afaik)
  UpdateException();
}

void ProcessorInterfaceManager::SetResetButton(bool set)
{
  SetInterrupt(INT_CAUSE_RST_BUTTON, !set);
}

void ProcessorInterfaceManager::ToggleResetButtonCallback(Core::System& system, u64 userdata,
                                                          s64 cyclesLate)
{
  system.GetProcessorInterface().SetResetButton(!!userdata);
}

void ProcessorInterfaceManager::IOSNotifyResetButtonCallback(Core::System& system, u64 userdata,
                                                             s64 cyclesLate)
{
  const auto ios = system.GetIOS();
  if (!ios)
    return;

  if (auto stm = ios->GetDeviceByName("/dev/stm/eventhook"))
    std::static_pointer_cast<IOS::HLE::STMEventHookDevice>(stm)->ResetButton();
}

void ProcessorInterfaceManager::IOSNotifyPowerButtonCallback(Core::System& system, u64 userdata,
                                                             s64 cyclesLate)
{
  const auto ios = system.GetIOS();
  if (!ios)
    return;

  if (auto stm = ios->GetDeviceByName("/dev/stm/eventhook"))
    std::static_pointer_cast<IOS::HLE::STMEventHookDevice>(stm)->PowerButton();
}

void ProcessorInterfaceManager::ResetButton_Tap()
{
  if (!Core::IsRunning(m_system))
    return;

  auto& core_timing = m_system.GetCoreTiming();
  core_timing.ScheduleEvent(0, m_event_type_toggle_reset_button, true, CoreTiming::FromThread::ANY);
  core_timing.ScheduleEvent(0, m_event_type_ios_notify_reset_button, 0,
                            CoreTiming::FromThread::ANY);
  core_timing.ScheduleEvent(m_system.GetSystemTimers().GetTicksPerSecond() / 2,
                            m_event_type_toggle_reset_button, false, CoreTiming::FromThread::ANY);
}

void ProcessorInterfaceManager::PowerButton_Tap()
{
  if (!Core::IsRunning(m_system))
    return;

  auto& core_timing = m_system.GetCoreTiming();
  core_timing.ScheduleEvent(0, m_event_type_ios_notify_power_button, 0,
                            CoreTiming::FromThread::ANY);
}

}  // namespace ProcessorInterface
