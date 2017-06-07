// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <vector>

#include <mbedtls/sha1.h>

#include "Common/Assert.h"
#include "Common/ChunkFile.h"
#include "Common/Crypto/AES.h"
#include "Common/Crypto/ec.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOSC.h"
#include "Core/ec_wii.h"

namespace IOS
{
namespace HLE
{
IOSC::IOSC(ConsoleType console_type)
{
  LoadDefaultEntries(console_type);
}

IOSC::~IOSC() = default;

ReturnCode IOSC::CreateObject(Handle* handle, ObjectType type, ObjectSubType subtype, u32 pid)
{
  auto iterator = FindFreeEntry();
  if (iterator == m_key_entries.end())
    return IOSC_FAIL_ALLOC;

  iterator->in_use = true;
  iterator->type = type;
  iterator->subtype = subtype;
  iterator->owner_mask = 1 << pid;

  *handle = GetHandleFromIterator(iterator);
  return IPC_SUCCESS;
}

ReturnCode IOSC::DeleteObject(Handle handle, u32 pid)
{
  if (IsDefaultHandle(handle) || !HasOwnership(handle, pid))
    return IOSC_EACCES;

  m_key_entries[handle].in_use = false;
  m_key_entries[handle].data.clear();
  return IPC_SUCCESS;
}

constexpr size_t AES128_KEY_SIZE = 0x10;
ReturnCode IOSC::ImportSecretKey(Handle dest_handle, Handle decrypt_handle, u8* iv,
                                 const u8* encrypted_key, u32 pid)
{
  if (!HasOwnership(dest_handle, pid) || !HasOwnership(decrypt_handle, pid) ||
      IsDefaultHandle(dest_handle))
  {
    return IOSC_EACCES;
  }

  auto* dest_entry = &m_key_entries[dest_handle];
  // TODO: allow other secret key subtypes
  if (dest_entry->type != TYPE_SECRET_KEY || dest_entry->subtype != SUBTYPE_AES128)
    return IOSC_INVALID_OBJTYPE;

  dest_entry->data.resize(AES128_KEY_SIZE);
  return Decrypt(decrypt_handle, iv, encrypted_key, AES128_KEY_SIZE, dest_entry->data.data(), pid);
}

constexpr size_t ECC233_PUBLIC_KEY_SIZE = 0x3c;
ReturnCode IOSC::ImportPublicKey(Handle dest_handle, const u8* public_key, u32 pid)
{
  if (!HasOwnership(dest_handle, pid) || IsDefaultHandle(dest_handle))
    return IOSC_EACCES;

  auto* dest_entry = &m_key_entries[dest_handle];
  // TODO: allow other public key subtypes
  if (dest_entry->type != TYPE_PUBLIC_KEY || dest_entry->subtype != SUBTYPE_ECC233)
    return IOSC_INVALID_OBJTYPE;

  dest_entry->data.assign(public_key, public_key + ECC233_PUBLIC_KEY_SIZE);
  return IPC_SUCCESS;
}

ReturnCode IOSC::ComputeSharedKey(Handle dest_handle, Handle private_handle, Handle public_handle,
                                  u32 pid)
{
  if (!HasOwnership(dest_handle, pid) || !HasOwnership(private_handle, pid) ||
      !HasOwnership(public_handle, pid) || IsDefaultHandle(dest_handle))
  {
    return IOSC_EACCES;
  }

  auto* dest_entry = &m_key_entries[dest_handle];
  const auto* private_entry = &m_key_entries[private_handle];
  const auto* public_entry = &m_key_entries[public_handle];
  if (dest_entry->type != TYPE_SECRET_KEY || dest_entry->subtype != SUBTYPE_AES128 ||
      private_entry->type != TYPE_SECRET_KEY || private_entry->subtype != SUBTYPE_ECC233 ||
      public_entry->type != TYPE_PUBLIC_KEY || public_entry->subtype != SUBTYPE_ECC233)
  {
    return IOSC_INVALID_OBJTYPE;
  }

  // Calculate the ECC shared secret.
  std::array<u8, 0x3c> shared_secret;
  point_mul(shared_secret.data(), private_entry->data.data(), public_entry->data.data());

  std::array<u8, 20> sha1;
  mbedtls_sha1(shared_secret.data(), shared_secret.size() / 2, sha1.data());

  dest_entry->data.resize(AES128_KEY_SIZE);
  std::copy_n(sha1.cbegin(), AES128_KEY_SIZE, dest_entry->data.begin());
  return IPC_SUCCESS;
}

ReturnCode IOSC::DecryptEncrypt(Common::AES::Mode mode, Handle key_handle, u8* iv, const u8* input,
                                size_t size, u8* output, u32 pid) const
{
  if (!HasOwnership(key_handle, pid))
    return IOSC_EACCES;

  const auto* entry = &m_key_entries[key_handle];
  if (entry->type != TYPE_SECRET_KEY || entry->subtype != SUBTYPE_AES128)
    return IOSC_INVALID_OBJTYPE;

  if (entry->data.size() != AES128_KEY_SIZE)
    return IOSC_FAIL_INTERNAL;

  const std::vector<u8> data =
      Common::AES::DecryptEncrypt(entry->data.data(), iv, input, size, mode);

  std::memcpy(output, data.data(), data.size());
  return IPC_SUCCESS;
}

ReturnCode IOSC::Encrypt(Handle key_handle, u8* iv, const u8* input, size_t size, u8* output,
                         u32 pid) const
{
  return DecryptEncrypt(Common::AES::Mode::Encrypt, key_handle, iv, input, size, output, pid);
}

ReturnCode IOSC::Decrypt(Handle key_handle, u8* iv, const u8* input, size_t size, u8* output,
                         u32 pid) const
{
  return DecryptEncrypt(Common::AES::Mode::Decrypt, key_handle, iv, input, size, output, pid);
}

ReturnCode IOSC::GetOwnership(Handle handle, u32* owner) const
{
  if (handle < m_key_entries.size() && m_key_entries[handle].in_use)
  {
    *owner = m_key_entries[handle].owner_mask;
    return IPC_SUCCESS;
  }
  return IOSC_EINVAL;
}

ReturnCode IOSC::SetOwnership(Handle handle, u32 new_owner, u32 pid)
{
  if (!HasOwnership(handle, pid))
    return IOSC_EACCES;

  m_key_entries[handle].owner_mask = new_owner;
  return IPC_SUCCESS;
}

void IOSC::LoadDefaultEntries(ConsoleType console_type)
{
  // TODO: add support for loading and writing to a BootMii / SEEPROM and OTP dump.

  const EcWii& ec = EcWii::GetInstance();

  m_key_entries[HANDLE_CONSOLE_KEY] = {TYPE_SECRET_KEY, SUBTYPE_ECC233,
                                       std::vector<u8>(ec.GetNGPriv(), ec.GetNGPriv() + 30), 3};

  // Unimplemented.
  m_key_entries[HANDLE_CONSOLE_ID] = {TYPE_DATA, SUBTYPE_DATA, std::vector<u8>(4), 0xFFFFFFF};
  m_key_entries[HANDLE_FS_KEY] = {TYPE_SECRET_KEY, SUBTYPE_AES128, std::vector<u8>(16), 5};
  m_key_entries[HANDLE_FS_MAC] = {TYPE_SECRET_KEY, SUBTYPE_MAC, std::vector<u8>(20), 5};

  switch (console_type)
  {
  case ConsoleType::Retail:
    m_key_entries[HANDLE_COMMON_KEY] = {TYPE_SECRET_KEY,
                                        SUBTYPE_AES128,
                                        {{0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4, 0x48,
                                          0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7}},
                                        3};
    break;
  case ConsoleType::RVT:
    m_key_entries[HANDLE_COMMON_KEY] = {TYPE_SECRET_KEY,
                                        SUBTYPE_AES128,
                                        {{0xa1, 0x60, 0x4a, 0x6a, 0x71, 0x23, 0xb5, 0x29, 0xae,
                                          0x8b, 0xec, 0x32, 0xc8, 0x16, 0xfc, 0xaa}},
                                        3};
    break;
  default:
    _assert_msg_(IOS, false, "Unknown console type");
    break;
  }

  // Unimplemented.
  m_key_entries[HANDLE_PRNG_KEY] = {TYPE_SECRET_KEY, SUBTYPE_AES128, std::vector<u8>(16), 3};

  m_key_entries[HANDLE_SD_KEY] = {TYPE_SECRET_KEY,
                                  SUBTYPE_AES128,
                                  {{0xab, 0x01, 0xb9, 0xd8, 0xe1, 0x62, 0x2b, 0x08, 0xaf, 0xba,
                                    0xd8, 0x4d, 0xbf, 0xc2, 0xa5, 0x5d}},
                                  3};

  // Unimplemented.
  m_key_entries[HANDLE_BOOT2_VERSION] = {TYPE_DATA, SUBTYPE_VERSION, std::vector<u8>(4), 3};
  m_key_entries[HANDLE_UNKNOWN_8] = {TYPE_DATA, SUBTYPE_VERSION, std::vector<u8>(4), 3};
  m_key_entries[HANDLE_UNKNOWN_9] = {TYPE_DATA, SUBTYPE_VERSION, std::vector<u8>(4), 3};
  m_key_entries[HANDLE_FS_VERSION] = {TYPE_DATA, SUBTYPE_VERSION, std::vector<u8>(4), 3};

  m_key_entries[HANDLE_NEW_COMMON_KEY] = {TYPE_SECRET_KEY,
                                          SUBTYPE_AES128,
                                          {{0x63, 0xb8, 0x2b, 0xb4, 0xf4, 0x61, 0x4e, 0x2e, 0x13,
                                            0xf2, 0xfe, 0xfb, 0xba, 0x4c, 0x9b, 0x7e}},
                                          3};
}

IOSC::KeyEntry::KeyEntry() = default;

IOSC::KeyEntry::KeyEntry(ObjectType type_, ObjectSubType subtype_, std::vector<u8>&& data_,
                         u32 owner_mask_)
    : in_use(true), type(type_), subtype(subtype_), data(std::move(data_)), owner_mask(owner_mask_)
{
}

IOSC::KeyEntries::iterator IOSC::FindFreeEntry()
{
  return std::find_if(m_key_entries.begin(), m_key_entries.end(),
                      [](const auto& entry) { return !entry.in_use; });
}

IOSC::Handle IOSC::GetHandleFromIterator(IOSC::KeyEntries::iterator iterator) const
{
  _assert_(iterator != m_key_entries.end());
  return static_cast<Handle>(iterator - m_key_entries.begin());
}

bool IOSC::HasOwnership(Handle handle, u32 pid) const
{
  u32 owner_mask;
  return GetOwnership(handle, &owner_mask) == IPC_SUCCESS && ((1 << pid) & owner_mask) != 0;
}

bool IOSC::IsDefaultHandle(Handle handle) const
{
  constexpr Handle last_default_handle = HANDLE_NEW_COMMON_KEY;
  return handle <= last_default_handle;
}

void IOSC::DoState(PointerWrap& p)
{
  for (auto& entry : m_key_entries)
    entry.DoState(p);
}

void IOSC::KeyEntry::DoState(PointerWrap& p)
{
  p.Do(in_use);
  p.Do(type);
  p.Do(subtype);
  p.Do(data);
  p.Do(owner_mask);
}
}  // namespace HLE
}  // namespace IOS
