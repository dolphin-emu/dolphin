// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/HW/DSPHLE/UCodes/GBA.h"

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Core/HW/DSP.h"
#include "Core/HW/DSPHLE/DSPHLE.h"
#include "Core/HW/DSPHLE/MailHandler.h"
#include "Core/HW/DSPHLE/UCodes/UCodes.h"

namespace DSP
{
namespace HLE
{
void ProcessGBACrypto(u32 address)
{
  // Nonce challenge (first read from GBA, hence already little-endian)
  const u32 challenge = HLEMemory_Read_U32LE(address);

  // Palette of pulsing logo on GBA during transmission [0,6]
  const u32 logo_palette = HLEMemory_Read_U32(address + 4);

  // Speed and direction of palette interpolation [-4,4]
  const u32 logo_speed_32 = HLEMemory_Read_U32(address + 8);

  // Length of JoyBoot program to upload
  const u32 length = HLEMemory_Read_U32(address + 12);

  // Address to return results to game
  const u32 dest_addr = HLEMemory_Read_U32(address + 16);

  // Unwrap key from challenge using 'sedo' magic number (to encrypt JoyBoot program)
  const u32 key = challenge ^ 0x6f646573;
  HLEMemory_Write_U32(dest_addr, key);

  // Pack palette parameters
  u16 palette_speed_coded;
  const s16 logo_speed = static_cast<s8>(logo_speed_32);
  if (logo_speed < 0)
    palette_speed_coded = ((-logo_speed + 2) * 2) | (logo_palette << 4);
  else if (logo_speed == 0)
    palette_speed_coded = (logo_palette * 2) | 0x70;
  else  // logo_speed > 0
    palette_speed_coded = ((logo_speed - 1) * 2) | (logo_palette << 4);

  // JoyBoot ROMs start with a padded header; this is the length beyond that header
  const s32 length_no_header = Common::AlignUp(length, 8) - 0x200;

  // The JoyBus protocol transmits in 4-byte packets while flipping a state flag;
  // so the GBA BIOS counts the program length in 8-byte packet-pairs
  const u16 packet_pair_count = (length_no_header < 0) ? 0 : length_no_header / 8;
  palette_speed_coded |= (packet_pair_count & 0x4000) >> 14;

  // Pack together encoded transmission parameters
  u32 t1 = (((packet_pair_count << 16) | 0x3f80) & 0x3f80ffff) * 2;
  t1 += (static_cast<s16>(static_cast<s8>(t1 >> 8)) & packet_pair_count) << 16;
  const u32 t2 = ((palette_speed_coded & 0xff) << 16) + (t1 & 0xff0000) + ((t1 >> 8) & 0xffff00);
  u32 t3 = palette_speed_coded << 16 | ((t2 << 8) & 0xff000000) | (t1 >> 16) | 0x80808080;

  // Wrap with 'Kawa' or 'sedo' (Kawasedo is the author of the BIOS cipher)
  t3 ^= ((t3 & 0x200) != 0 ? 0x6f646573 : 0x6177614b);
  HLEMemory_Write_U32(dest_addr + 4, t3);

  // Done!
  DEBUG_LOG(DSPHLE, "\n%08x -> challenge: %08x, len: %08x, dest_addr: %08x, "
                    "palette: %08x, speed: %08x key: %08x, auth_code: %08x",
            address, challenge, length, dest_addr, logo_palette, logo_speed_32, key, t3);
}

GBAUCode::GBAUCode(DSPHLE* dsphle, u32 crc) : UCodeInterface(dsphle, crc)
{
}

GBAUCode::~GBAUCode()
{
  m_mail_handler.Clear();
}

void GBAUCode::Initialize()
{
  m_mail_handler.PushMail(DSP_INIT);
}

void GBAUCode::Update()
{
  // check if we have to send something
  if (!m_mail_handler.IsEmpty())
  {
    DSP::GenerateDSPInterruptFromDSPEmu(DSP::INT_DSP);
  }
}

void GBAUCode::HandleMail(u32 mail)
{
  static bool nextmail_is_mramaddr = false;
  static bool calc_done = false;

  if (m_upload_setup_in_progress)
  {
    PrepareBootUCode(mail);
  }
  else if ((mail >> 16 == 0xabba) && !nextmail_is_mramaddr)
  {
    nextmail_is_mramaddr = true;
  }
  else if (nextmail_is_mramaddr)
  {
    nextmail_is_mramaddr = false;

    ProcessGBACrypto(mail);

    calc_done = true;
    m_mail_handler.PushMail(DSP_DONE);
  }
  else if ((mail >> 16 == 0xcdd1) && calc_done)
  {
    switch (mail & 0xffff)
    {
    case 1:
      m_upload_setup_in_progress = true;
      break;
    case 2:
      m_dsphle->SetUCode(UCODE_ROM);
      break;
    default:
      WARN_LOG(DSPHLE, "GBAUCode - unknown 0xcdd1 command: %08x", mail);
      break;
    }
  }
  else
  {
    WARN_LOG(DSPHLE, "GBAUCode - unknown command: %08x", mail);
  }
}
}  // namespace HLE
}  // namespace DSP
