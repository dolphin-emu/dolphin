// Copyright 2011 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DSPHLE/DSPHLE.h"

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Core/Core.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"
#include "Core/HW/SystemTimers.h"

#include "VideoCommon/VR.h"
#include "VideoCommon/VideoConfig.h"

namespace DSP
{
namespace HLE
{
DSPHLE::DSPHLE() = default;

DSPHLE::~DSPHLE() = default;

bool DSPHLE::Initialize(bool wii, bool dsp_thread)
{
  m_wii = wii;
  m_ucode = nullptr;
  m_last_ucode = nullptr;
  m_halt = false;
  m_assert_interrupt = false;

  SetUCode(UCODE_ROM);
  m_dsp_control.DSPHalt = 1;
  m_dsp_control.DSPInit = 1;

  m_dsp_state.Reset();

  return true;
}

void DSPHLE::DSP_StopSoundStream()
{
}

void DSPHLE::Shutdown()
{
  m_ucode = nullptr;
}

void DSPHLE::DSP_Update(int cycles)
{
  if (m_ucode != nullptr)
    m_ucode->Update();
}

u32 DSPHLE::DSP_UpdateRate()
{
  // AX HLE uses 3ms (Wii) or 5ms (GC) timing period
  // But to be sure, just update the HLE every ms.
  return SystemTimers::GetTicksPerSecond() / 1000;
  // if (m_pUCode != nullptr)
  //{
  //	return (u32)((SystemTimers::GetTicksPerSecond() / (1000 /
  //SConfig::GetInstance().m_AudioSlowDown)) * m_pUCode->GetUpdateMs());
  //}
  // else
  //{
  //	return SystemTimers::GetTicksPerSecond() / 1000;
  //}
}

void DSPHLE::SendMailToDSP(u32 mail)
{
  if (m_ucode != nullptr)
  {
    DEBUG_LOG(DSP_MAIL, "CPU writes 0x%08x", mail);
    m_ucode->HandleMail(mail);
  }
}

void DSPHLE::SetUCode(u32 crc)
{
  m_mail_handler.Clear();
  m_ucode = UCodeFactory(crc, this, m_wii);
  m_ucode->Initialize();
}

// TODO do it better?
// Assumes that every odd call to this func is by the persistent ucode.
// Even callers are deleted.
void DSPHLE::SwapUCode(u32 crc)
{
  m_mail_handler.Clear();

  if (m_last_ucode == nullptr)
  {
    m_last_ucode = std::move(m_ucode);
    m_ucode = UCodeFactory(crc, this, m_wii);
    m_ucode->Initialize();
  }
  else
  {
    m_ucode = std::move(m_last_ucode);
  }
}

void DSPHLE::DoState(PointerWrap& p)
{
  bool is_hle = true;
  p.Do(is_hle);
  if (!is_hle && p.GetMode() == PointerWrap::MODE_READ)
  {
    Core::DisplayMessage("State is incompatible with current DSP engine. Aborting load state.",
                         3000);
    p.SetMode(PointerWrap::MODE_VERIFY);
    return;
  }

  p.DoPOD(m_dsp_control);
  p.DoPOD(m_dsp_state);

  int ucode_crc = UCodeInterface::GetCRC(m_ucode.get());
  int ucode_crc_before_load = ucode_crc;
  int last_ucode_crc = UCodeInterface::GetCRC(m_last_ucode.get());
  int last_ucode_crc_before_load = last_ucode_crc;

  p.Do(ucode_crc);
  p.Do(last_ucode_crc);

  // if a different type of ucode was being used when the savestate was created,
  // we have to reconstruct the old type of ucode so that we have a valid thing to call DoState on.
  const bool same_ucode = ucode_crc == ucode_crc_before_load;
  const bool same_last_ucode = last_ucode_crc == last_ucode_crc_before_load;
  auto ucode = same_ucode ? std::move(m_ucode) : UCodeFactory(ucode_crc, this, m_wii);
  auto last_ucode =
      same_last_ucode ? std::move(m_last_ucode) : UCodeFactory(last_ucode_crc, this, m_wii);

  if (ucode)
    ucode->DoState(p);
  if (last_ucode)
    last_ucode->DoState(p);

  m_ucode = std::move(ucode);
  m_last_ucode = std::move(last_ucode);

  m_mail_handler.DoState(p);
}

// Mailbox functions
u16 DSPHLE::DSP_ReadMailBoxHigh(bool cpu_mailbox)
{
  if (cpu_mailbox)
  {
    return (m_dsp_state.cpu_mailbox >> 16) & 0xFFFF;
  }
  else
  {
    return AccessMailHandler().ReadDSPMailboxHigh();
  }
}

u16 DSPHLE::DSP_ReadMailBoxLow(bool cpu_mailbox)
{
  if (cpu_mailbox)
  {
    return m_dsp_state.cpu_mailbox & 0xFFFF;
  }
  else
  {
    return AccessMailHandler().ReadDSPMailboxLow();
  }
}

void DSPHLE::DSP_WriteMailBoxHigh(bool cpu_mailbox, u16 value)
{
  if (cpu_mailbox)
  {
    m_dsp_state.cpu_mailbox = (m_dsp_state.cpu_mailbox & 0xFFFF) | (value << 16);
  }
  else
  {
    PanicAlert("CPU can't write %08x to DSP mailbox", value);
  }
}

void DSPHLE::DSP_WriteMailBoxLow(bool cpu_mailbox, u16 value)
{
  if (cpu_mailbox)
  {
    m_dsp_state.cpu_mailbox = (m_dsp_state.cpu_mailbox & 0xFFFF0000) | value;
    SendMailToDSP(m_dsp_state.cpu_mailbox);
    // Mail sent so clear MSB to show that it is progressed
    m_dsp_state.cpu_mailbox &= 0x7FFFFFFF;
  }
  else
  {
    PanicAlert("CPU can't write %08x to DSP mailbox", value);
  }
}

// Other DSP functions
u16 DSPHLE::DSP_WriteControlRegister(u16 value)
{
  DSP::UDSPControl temp(value);

  if (temp.DSPReset)
  {
    SetUCode(UCODE_ROM);
    temp.DSPReset = 0;
  }
  if (temp.DSPInit == 0)
  {
    // copy 128 byte from ARAM 0x000000 to IMEM
    SetUCode(UCODE_INIT_AUDIO_SYSTEM);
    temp.DSPInitCode = 0;
  }

  m_dsp_control.Hex = temp.Hex;
  return m_dsp_control.Hex;
}

u16 DSPHLE::DSP_ReadControlRegister()
{
  return m_dsp_control.Hex;
}

void DSPHLE::PauseAndLock(bool do_lock, bool unpause_on_unlock)
{
}
}  // namespace HLE
}  // namespace DSP
