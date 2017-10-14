// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Based off of twintig http://git.infradead.org/?p=users/segher/wii.git
// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#include "Core/ec_wii.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <string>

#include <mbedtls/sha1.h>

#include "Common/CommonTypes.h"
#include "Common/Crypto/ec.h"
#include "Common/File.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

constexpr u32 default_NG_key_id = 0x6AAB8C59;

constexpr u8 default_NG_priv[] = {
    0x00, 0xAB, 0xEE, 0xC1, 0xDD, 0xB4, 0xA6, 0x16, 0x6B, 0x70, 0xFD, 0x7E, 0x56, 0x67, 0x70,
    0x57, 0x55, 0x27, 0x38, 0xA3, 0x26, 0xC5, 0x46, 0x16, 0xF7, 0x62, 0xC9, 0xED, 0x73, 0xF2,
};

constexpr u8 default_NG_sig[] = {
    // R
    0x00, 0xD8, 0x81, 0x63, 0xB2, 0x00, 0x6B, 0x0B, 0x54, 0x82, 0x88, 0x63, 0x81, 0x1C, 0x00, 0x71,
    0x12, 0xED, 0xB7, 0xFD, 0x21, 0xAB, 0x0E, 0x50, 0x0E, 0x1F, 0xBF, 0x78, 0xAD, 0x37,
    // S
    0x00, 0x71, 0x8D, 0x82, 0x41, 0xEE, 0x45, 0x11, 0xC7, 0x3B, 0xAC, 0x08, 0xB6, 0x83, 0xDC, 0x05,
    0xB8, 0xA8, 0x90, 0x1F, 0xA8, 0x2A, 0x0E, 0x4E, 0x76, 0xEF, 0x44, 0x72, 0x99, 0xF8,
};

static void MakeBlankSigECCert(u8* cert_out, const char* signer, const char* name,
                               const u8* private_key, u32 key_id)
{
  memset(cert_out, 0, 0x180);
  *(u32*)cert_out = Common::swap32(0x10002);

  strncpy((char*)cert_out + 0x80, signer, 0x40);
  *(u32*)(cert_out + 0xc0) = Common::swap32(2);
  strncpy((char*)cert_out + 0xc4, name, 0x40);
  *(u32*)(cert_out + 0x104) = Common::swap32(key_id);
  ec_priv_to_pub(private_key, cert_out + 0x108);
}

// ng_cert_out is a pointer to a 0x180 byte buffer that will contain the device-unique certificate
// NG_id is the device-unique id to use
// NG_key_id is the device-unique key_id to use
// NG_priv is the device-unique private key to use
// NG_sig is the device-unique signature blob (from issuer) to use
// if NG_priv iis nullptr or NG_sig is nullptr or NG_id is 0 or NG_key_id is 0, default values
// will be used for all of them
void MakeNGCert(u8* ng_cert_out, u32 NG_id, u32 NG_key_id, const u8* NG_priv, const u8* NG_sig)
{
  char name[64];
  if ((NG_id == 0) || (NG_key_id == 0) || (NG_priv == nullptr) || (NG_sig == nullptr))
  {
    NG_id = DEFAULT_WII_DEVICE_ID;
    NG_key_id = default_NG_key_id;
    NG_priv = default_NG_priv;
    NG_sig = default_NG_sig;
  }

  sprintf(name, "NG%08x", NG_id);
  MakeBlankSigECCert(ng_cert_out, "Root-CA00000001-MS00000002", name, NG_priv, NG_key_id);
  memcpy(ng_cert_out + 4, NG_sig, 60);
}

// get_ap_sig_and_cert

// sig_out is a pointer to a 0x3c byte buffer which will be filled with the data payload's signature
// ap_cert_out is a pointer to a 0x180 byte buffer which will be filled with the temporal AP
// certificate
// title_id is the title responsible for the signing
// data is a pointer to the buffer of data to sign
// data_size is the length of the buffer
// NG_priv is the device-unique private key to use
// NG_id is the device-unique id to use
// if NG_priv is nullptr or NG_id is 0, it will use builtin defaults
void MakeAPSigAndCert(u8* sig_out, u8* ap_cert_out, u64 title_id, u8* data, u32 data_size,
                      const u8* NG_priv, u32 NG_id)
{
  u8 hash[20];
  u8 ap_priv[30];
  char signer[64];
  char name[64];

  if ((NG_id == 0) || (NG_priv == nullptr))
  {
    NG_priv = default_NG_priv;
    NG_id = DEFAULT_WII_DEVICE_ID;
  }

  memset(ap_priv, 0, 0x1e);
  ap_priv[0x1d] = 1;
  // setup random ap_priv here if desired
  // get_rand_bytes(ap_priv, 0x1e);
  // ap_priv[0] &= 1;

  memset(ap_cert_out + 4, 0, 60);

  sprintf(signer, "Root-CA00000001-MS00000002-NG%08x", NG_id);
  sprintf(name, "AP%016" PRIx64, title_id);
  MakeBlankSigECCert(ap_cert_out, signer, name, ap_priv, 0);

  mbedtls_sha1(ap_cert_out + 0x80, 0x100, hash);
  generate_ecdsa(ap_cert_out + 4, ap_cert_out + 34, NG_priv, hash);

  mbedtls_sha1(data, data_size, hash);
  generate_ecdsa(sig_out, sig_out + 30, ap_priv, hash);
}

EcWii::EcWii()
{
  bool init = true;
  std::string keys_path = File::GetUserPath(D_WIIROOT_IDX) + "/keys.bin";
  if (File::Exists(keys_path))
  {
    File::IOFile keys_f(keys_path, "rb");
    if (keys_f.IsOpen())
    {
      if (keys_f.ReadBytes(&BootMiiKeysBin, sizeof(BootMiiKeysBin)))
      {
        init = false;

        INFO_LOG(IOS_ES, "Successfully loaded keys.bin created by: %s", BootMiiKeysBin.creator);
      }
      else
      {
        ERROR_LOG(IOS_ES, "Failed to read keys.bin, check it is the correct size of %08zX bytes.",
                  sizeof(BootMiiKeysBin));
      }
    }
    else
    {
      ERROR_LOG(IOS_ES, "Failed to open keys.bin, maybe a permissions error or it is in use?");
    }
  }
  else
  {
    ERROR_LOG(
        IOS_ES,
        "%s could not be found. Using default values. We recommend you grab keys.bin from BootMii.",
        keys_path.c_str());
  }

  if (init)
    InitDefaults();
}

EcWii::~EcWii()
{
}

u32 EcWii::GetNGID() const
{
  return Common::swap32(BootMiiKeysBin.ng_id);
}

u32 EcWii::GetNGKeyID() const
{
  return Common::swap32(BootMiiKeysBin.ng_key_id);
}

const u8* EcWii::GetNGPriv() const
{
  return BootMiiKeysBin.ng_priv;
}

const u8* EcWii::GetNGSig() const
{
  return BootMiiKeysBin.ng_sig;
}

const u8* EcWii::GetBackupKey() const
{
  return BootMiiKeysBin.backup_key;
}

void EcWii::InitDefaults()
{
  memset(&BootMiiKeysBin, 0, sizeof(BootMiiKeysBin));

  BootMiiKeysBin.ng_id = Common::swap32(DEFAULT_WII_DEVICE_ID);
  BootMiiKeysBin.ng_key_id = Common::swap32(default_NG_key_id);

  memcpy(BootMiiKeysBin.ng_priv, default_NG_priv, sizeof(BootMiiKeysBin.ng_priv));
  memcpy(BootMiiKeysBin.ng_sig, default_NG_sig, sizeof(BootMiiKeysBin.ng_sig));
}

EcWii& EcWii::GetInstance()
{
  static EcWii m_Instance;
  return (m_Instance);
}
