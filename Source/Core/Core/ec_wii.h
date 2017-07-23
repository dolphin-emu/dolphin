// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Based off of twintig http://git.infradead.org/?p=users/segher/wii.git
// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

/*
 *
 * Structs for keys.bin taken from:
 *
 * mini - a Free Software replacement for the Nintendo/BroadOn IOS.
 * crypto hardware support
 *
 * Copyright (C) 2008, 2009 Haxx Enterprises <bushing@gmail.com>
 * Copyright (C) 2008, 2009 Sven Peter <svenpeter@gmail.com>
 * Copyright (C) 2008, 2009 Hector Martin "marcan" <marcan@marcansoft.com>
 *
 * # This code is licensed to you under the terms of the GNU GPL, version 2;
 * # see file COPYING or http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt
 */

#pragma once

#include "Common/CommonTypes.h"

constexpr u32 DEFAULT_WII_DEVICE_ID = 0x0403AC68;

void MakeNGCert(u8* ng_cert_out, u32 NG_id, u32 NG_key_id, const u8* NG_priv, const u8* NG_sig);
void MakeAPSigAndCert(u8* sig_out, u8* ap_cert_out, u64 title_id, u8* data, u32 data_size,
                      const u8* NG_priv, u32 NG_id);

class EcWii
{
public:
  EcWii();
  ~EcWii();
  static EcWii& GetInstance();
  u32 GetNGID() const;
  u32 GetNGKeyID() const;
  const u8* GetNGPriv() const;
  const u8* GetNGSig() const;
  const u8* GetBackupKey() const;

private:
  void InitDefaults();

#pragma pack(push, 1)
  typedef struct
  {
    u8 boot2version;
    u8 unknown1;
    u8 unknown2;
    u8 pad;
    u32 update_tag;
    u16 checksum;
  }
#ifndef _WIN32
  __attribute__((packed))
#endif
  eep_ctr_t;

  struct
  {
    u8 creator[0x100];    // 0x000
    u8 boot1_hash[0x14];  // 0x100
    u8 common_key[0x10];  // 0x114
    u32 ng_id;            // 0x124
    union
    {
      struct
      {
        u8 ng_priv[0x1e];  // 0x128
        u8 pad1[0x12];
      };

      struct
      {
        u8 pad2[0x1c];
        u8 nand_hmac[0x14];  // 0x144
      };
    };
    u8 nand_key[0x10];    // 0x158
    u8 backup_key[0x10];  // 0x168
    u32 unk1;             // 0x178
    u32 unk2;             // 0x17C
    u8 eeprom_pad[0x80];  // 0x180

    u32 ms_id;                 // 0x200
    u32 ca_id;                 // 0x204
    u32 ng_key_id;             // 0x208
    u8 ng_sig[0x3c];           // 0x20c
    eep_ctr_t counters[0x02];  // 0x248
    u8 fill[0x18];             // 0x25c
    u8 korean_key[0x10];       // 0x274
    u8 pad3[0x74];             // 0x284
    u16 prng_seed[0x02];       // 0x2F8
    u8 pad4[0x04];             // 0x2FC

    u8 crack_pad[0x100];  // 0x300

  }

#ifndef _WIN32
  __attribute__((packed))
#endif

  BootMiiKeysBin;

#pragma pack(pop)
};
