// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <cstring>
#include <vector>

#include <mbedtls/aes.h>

#include "Common/Assert.h"
#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/ec_wii.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
constexpr u8 s_key_sd[0x10] = {0xab, 0x01, 0xb9, 0xd8, 0xe1, 0x62, 0x2b, 0x08,
                               0xaf, 0xba, 0xd8, 0x4d, 0xbf, 0xc2, 0xa5, 0x5d};
constexpr u8 s_key_ecc[0x1e] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
constexpr u8 s_key_empty[0x10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// default key table
constexpr const u8* s_key_table[11] = {
    s_key_ecc,    // ECC Private Key
    s_key_empty,  // Console ID
    s_key_empty,  // NAND AES Key
    s_key_empty,  // NAND HMAC
    s_key_empty,  // Common Key
    s_key_empty,  // PRNG seed
    s_key_sd,     // SD Key
    s_key_empty,  // Unknown
    s_key_empty,  // Unknown
    s_key_empty,  // Unknown
    s_key_empty,  // Unknown
};

void ES::DecryptContent(u32 key_index, u8* iv, u8* input, u32 size, u8* new_iv, u8* output)
{
  mbedtls_aes_context AES_ctx;
  mbedtls_aes_setkey_dec(&AES_ctx, s_key_table[key_index], 128);
  memcpy(new_iv, iv, 16);
  mbedtls_aes_crypt_cbc(&AES_ctx, MBEDTLS_AES_DECRYPT, size, new_iv, input, output);
}

IPCCommandResult ES::GetConsoleID(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1))
    return GetDefaultReply(ES_EINVAL);

  const EcWii& ec = EcWii::GetInstance();
  INFO_LOG(IOS_ES, "IOCTL_ES_GETDEVICEID %08X", ec.GetNGID());
  Memory::Write_U32(ec.GetNGID(), request.io_vectors[0].address);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::Encrypt(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 2))
    return GetDefaultReply(ES_EINVAL);

  u32 keyIndex = Memory::Read_U32(request.in_vectors[0].address);
  u8* IV = Memory::GetPointer(request.in_vectors[1].address);
  u8* source = Memory::GetPointer(request.in_vectors[2].address);
  u32 size = request.in_vectors[2].size;
  u8* newIV = Memory::GetPointer(request.io_vectors[0].address);
  u8* destination = Memory::GetPointer(request.io_vectors[1].address);

  mbedtls_aes_context AES_ctx;
  mbedtls_aes_setkey_enc(&AES_ctx, s_key_table[keyIndex], 128);
  memcpy(newIV, IV, 16);
  mbedtls_aes_crypt_cbc(&AES_ctx, MBEDTLS_AES_ENCRYPT, size, newIV, source, destination);

  _dbg_assert_msg_(IOS_ES, keyIndex == 6,
                   "IOCTL_ES_ENCRYPT: Key type is not SD, data will be crap");
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::Decrypt(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 2))
    return GetDefaultReply(ES_EINVAL);

  u32 keyIndex = Memory::Read_U32(request.in_vectors[0].address);
  u8* IV = Memory::GetPointer(request.in_vectors[1].address);
  u8* source = Memory::GetPointer(request.in_vectors[2].address);
  u32 size = request.in_vectors[2].size;
  u8* newIV = Memory::GetPointer(request.io_vectors[0].address);
  u8* destination = Memory::GetPointer(request.io_vectors[1].address);

  DecryptContent(keyIndex, IV, source, size, newIV, destination);

  _dbg_assert_msg_(IOS_ES, keyIndex == 6,
                   "IOCTL_ES_DECRYPT: Key type is not SD, data will be crap");
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::CheckKoreaRegion(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0))
    return GetDefaultReply(ES_EINVAL);

  // note by DacoTaco : name is unknown, I just tried to name it SOMETHING.
  // IOS70 has this to let system menu 4.2 check if the console is region changed. it returns
  // -1017
  // if the IOS didn't find the Korean keys and 0 if it does. 0 leads to a error 003
  INFO_LOG(IOS_ES, "IOCTL_ES_CHECKKOREAREGION: Title checked for Korean keys.");
  return GetDefaultReply(ES_EINVAL);
}

IPCCommandResult ES::GetDeviceCertificate(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1) || request.io_vectors[0].size != 0x180)
    return GetDefaultReply(ES_EINVAL);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETDEVICECERT");
  u8* destination = Memory::GetPointer(request.io_vectors[0].address);

  const EcWii& ec = EcWii::GetInstance();
  MakeNGCert(destination, ec.GetNGID(), ec.GetNGKeyID(), ec.GetNGPriv(), ec.GetNGSig());
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::Sign(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 2))
    return GetDefaultReply(ES_EINVAL);

  INFO_LOG(IOS_ES, "IOCTL_ES_SIGN");
  u8* ap_cert_out = Memory::GetPointer(request.io_vectors[1].address);
  u8* data = Memory::GetPointer(request.in_vectors[0].address);
  u32 data_size = request.in_vectors[0].size;
  u8* sig_out = Memory::GetPointer(request.io_vectors[0].address);

  if (!GetTitleContext().active)
    return GetDefaultReply(ES_EINVAL);

  const EcWii& ec = EcWii::GetInstance();
  MakeAPSigAndCert(sig_out, ap_cert_out, GetTitleContext().tmd.GetTitleId(), data, data_size,
                   ec.GetNGPriv(), ec.GetNGID());

  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
