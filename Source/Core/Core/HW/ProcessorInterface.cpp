// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/ProcessorInterface.h"

#include <cstdio>
#include <memory>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Core/Core.h"
#include "Core/CoreTiming.h"
#include "Core/HW/MMIO.h"
#include "Core/HW/SystemTimers.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/STM/STM.h"
#include "Core/PowerPC/PowerPC.h"

namespace ProcessorInterface
{
// STATE_TO_SAVE
u32 m_InterruptCause;
u32 m_InterruptMask;
// addresses for CPU fifo accesses
u32 Fifo_CPUBase;
u32 Fifo_CPUEnd;
u32 Fifo_CPUWritePointer;

static u32 m_Fifo_Reset;
static u32 m_ResetCode;
static u32 m_FlipperRev;
static u32 m_Unknown;

// ID and callback for scheduling reset button presses/releases
static CoreTiming::EventType* toggleResetButton;
static void ToggleResetButtonCallback(u64 userdata, s64 cyclesLate);

static CoreTiming::EventType* iosNotifyResetButton;
static void IOSNotifyResetButtonCallback(u64 userdata, s64 cyclesLate);

static CoreTiming::EventType* iosNotifyPowerButton;
static void IOSNotifyPowerButtonCallback(u64 userdata, s64 cyclesLate);

// Let the PPC know that an external exception is set/cleared
void UpdateException();

void DoState(PointerWrap& p)
{
  p.Do(m_InterruptMask);
  p.Do(m_InterruptCause);
  p.Do(Fifo_CPUBase);
  p.Do(Fifo_CPUEnd);
  p.Do(Fifo_CPUWritePointer);
  p.Do(m_Fifo_Reset);
  p.Do(m_ResetCode);
  p.Do(m_FlipperRev);
  p.Do(m_Unknown);
}

void Init()
{
  m_InterruptMask = 0;
  m_InterruptCause = 0;

  Fifo_CPUBase = 0;
  Fifo_CPUEnd = 0;
  Fifo_CPUWritePointer = 0;
  /*
  Previous Flipper IDs:
  0x046500B0 = A
  0x146500B1 = B
  */
  m_FlipperRev = 0x246500B1;  // revision C
  m_Unknown = 0;

  m_ResetCode = 0;  // Cold reset
  m_InterruptCause = INT_CAUSE_RST_BUTTON | INT_CAUSE_VI;

  toggleResetButton = CoreTiming::RegisterEvent("ToggleResetButton", ToggleResetButtonCallback);
  iosNotifyResetButton =
      CoreTiming::RegisterEvent("IOSNotifyResetButton", IOSNotifyResetButtonCallback);
  iosNotifyPowerButton =
      CoreTiming::RegisterEvent("IOSNotifyPowerButton", IOSNotifyPowerButtonCallback);
}

void RegisterMMIO(MMIO::Mapping* mmio, u32 base)
{
  mmio->Register(base | PI_INTERRUPT_CAUSE, MMIO::DirectRead<u32>(&m_InterruptCause),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   m_InterruptCause &= ~val;
                   UpdateException();
                 }));

  mmio->Register(base | PI_INTERRUPT_MASK, MMIO::DirectRead<u32>(&m_InterruptMask),
                 MMIO::ComplexWrite<u32>([](u32, u32 val) {
                   m_InterruptMask = val;
                   UpdateException();
                 }));

  mmio->Register(base | PI_FIFO_BASE, MMIO::DirectRead<u32>(&Fifo_CPUBase),
                 MMIO::DirectWrite<u32>(&Fifo_CPUBase, 0xFFFFFFE0));

  mmio->Register(base | PI_FIFO_END, MMIO::DirectRead<u32>(&Fifo_CPUEnd),
                 MMIO::DirectWrite<u32>(&Fifo_CPUEnd, 0xFFFFFFE0));

  mmio->Register(base | PI_FIFO_WPTR, MMIO::DirectRead<u32>(&Fifo_CPUWritePointer),
                 MMIO::DirectWrite<u32>(&Fifo_CPUWritePointer, 0xFFFFFFE0));

  mmio->Register(base | PI_FIFO_RESET, MMIO::InvalidRead<u32>(),
                 MMIO::ComplexWrite<u32>(
                     [](u32, u32 val) { WARN_LOG(PROCESSORINTERFACE, "Fifo reset (%08x)", val); }));

  mmio->Register(base | PI_RESET_CODE, MMIO::DirectRead<u32>(&m_ResetCode),
                 MMIO::DirectWrite<u32>(&m_ResetCode));

  mmio->Register(base | PI_FLIPPER_REV, MMIO::DirectRead<u32>(&m_FlipperRev),
                 MMIO::InvalidWrite<u32>());

  // 16 bit reads are based on 32 bit reads.
  for (int i = 0; i < 0x1000; i += 4)
  {
    mmio->Register(base | i, MMIO::ReadToLarger<u16>(mmio, base | i, 16),
                   MMIO::InvalidWrite<u16>());
    mmio->Register(base | (i + 2), MMIO::ReadToLarger<u16>(mmio, base | i, 0),
                   MMIO::InvalidWrite<u16>());
  }
}

void UpdateException()
{
  if ((m_InterruptCause & m_InterruptMask) != 0)
    PowerPC::ppcState.Exceptions |= EXCEPTION_EXTERNAL_INT;
  else
    PowerPC::ppcState.Exceptions &= ~EXCEPTION_EXTERNAL_INT;
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

void SetInterrupt(u32 cause_mask, bool set)
{
  DEBUG_ASSERT_MSG(POWERPC, Core::IsCPUThread(), "SetInterrupt from wrong thread");

  if (set && !(m_InterruptCause & cause_mask))
  {
    DEBUG_LOG(PROCESSORINTERFACE, "Setting Interrupt %s (set)", Debug_GetInterruptName(cause_mask));
  }

  if (!set && (m_InterruptCause & cause_mask))
  {
    DEBUG_LOG(PROCESSORINTERFACE, "Setting Interrupt %s (clear)",
              Debug_GetInterruptName(cause_mask));
  }

  if (set)
    m_InterruptCause |= cause_mask;
  else
    m_InterruptCause &= ~cause_mask;  // is there any reason to have this possibility?
  // F|RES: i think the hw devices reset the interrupt in the PI to 0
  // if the interrupt cause is eliminated. that isn't done by software (afaik)
  UpdateException();
}

static void SetResetButton(bool set)
{
  SetInterrupt(INT_CAUSE_RST_BUTTON, !set);
}

static void ToggleResetButtonCallback(u64 userdata, s64 cyclesLate)
{
  SetResetButton(!!userdata);
}

static void IOSNotifyResetButtonCallback(u64 userdata, s64 cyclesLate)
{
  const auto ios = IOS::HLE::GetIOS();
  if (!ios)
    return;

  auto stm = ios->GetDeviceByName("/dev/stm/eventhook");
  if (stm)
    std::static_pointer_cast<IOS::HLE::Device::STMEventHook>(stm)->ResetButton();
}

static void IOSNotifyPowerButtonCallback(u64 userdata, s64 cyclesLate)
{
  const auto ios = IOS::HLE::GetIOS();
  if (!ios)
    return;

  auto stm = ios->GetDeviceByName("/dev/stm/eventhook");
  if (stm)
    std::static_pointer_cast<IOS::HLE::Device::STMEventHook>(stm)->PowerButton();
}

void ResetButton_Tap()
{
  if (!Core::IsRunning())
    return;
  CoreTiming::ScheduleEvent(0, toggleResetButton, true, CoreTiming::FromThread::ANY);
  CoreTiming::ScheduleEvent(0, iosNotifyResetButton, 0, CoreTiming::FromThread::ANY);
  CoreTiming::ScheduleEvent(SystemTimers::GetTicksPerSecond() / 2, toggleResetButton, false,
                            CoreTiming::FromThread::ANY);
}

void PowerButton_Tap()
{
  if (!Core::IsRunning())
    return;
  CoreTiming::ScheduleEvent(0, iosNotifyPowerButton, 0, CoreTiming::FromThread::ANY);
}

}  // namespace ProcessorInterface
