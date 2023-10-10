// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Implementation of an IOSC-like API, but much simpler since we only support actual keys.

#pragma once

#include <array>
#include <cstddef>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/Crypto/AES.h"
#include "Common/Crypto/ec.h"

class PointerWrap;

namespace IOS
{
namespace ES
{
class CertReader;
}  // namespace ES

enum class SignatureType : u32
{
  RSA4096 = 0x00010000,
  RSA2048 = 0x00010001,
  ECC = 0x00010002,
};

enum class PublicKeyType : u32
{
  RSA4096 = 0,
  RSA2048 = 1,
  ECC = 2,
};

#pragma pack(push, 4)
struct SignatureRSA4096
{
  SignatureType type;
  u8 sig[0x200];
  u8 fill[0x3c];
  char issuer[0x40];
};
static_assert(sizeof(SignatureRSA4096) == 0x280, "Wrong size for SignatureRSA4096");

struct SignatureRSA2048
{
  SignatureType type;
  u8 sig[0x100];
  u8 fill[0x3c];
  char issuer[0x40];
};
static_assert(sizeof(SignatureRSA2048) == 0x180, "Wrong size for SignatureRSA2048");

struct SignatureECC
{
  SignatureType type;
  Common::ec::Signature sig;
  u8 fill[0x40];
  char issuer[0x40];
};
static_assert(sizeof(SignatureECC) == 0xc0, "Wrong size for SignatureECC");

// Source: https://wiibrew.org/wiki/Certificate_chain
struct CertHeader
{
  PublicKeyType public_key_type;
  char name[0x40];
  u32 id;
};

using RSA2048PublicKey = std::array<u8, 0x100>;

struct CertRSA4096RSA2048
{
  SignatureRSA4096 signature;
  CertHeader header;
  RSA2048PublicKey public_key;
  u8 exponent[0x4];
  u8 pad[0x34];
};
static_assert(sizeof(CertRSA4096RSA2048) == 0x400, "Wrong size for CertRSA4096RSA2048");

struct CertRSA2048RSA2048
{
  SignatureRSA2048 signature;
  CertHeader header;
  RSA2048PublicKey public_key;
  u8 exponent[0x4];
  u8 pad[0x34];
};
static_assert(sizeof(CertRSA2048RSA2048) == 0x300, "Wrong size for CertRSA2048RSA2048");

/// Used for device certificates
struct CertRSA2048ECC
{
  SignatureRSA2048 signature;
  CertHeader header;
  Common::ec::PublicKey public_key;
  std::array<u8, 60> padding;
};
static_assert(sizeof(CertRSA2048ECC) == 0x240, "Wrong size for CertRSA2048ECC");

/// Used for device signed certificates
struct CertECC
{
  SignatureECC signature;
  CertHeader header;
  Common::ec::PublicKey public_key;
  std::array<u8, 60> padding;
};
static_assert(sizeof(CertECC) == 0x180, "Wrong size for CertECC");
#pragma pack(pop)

namespace HLE
{
enum ReturnCode : s32;

class IOSC final
{
public:
  using Handle = u32;

  enum class ConsoleType
  {
    Retail,
    RVT,
  };

  // We use the same default key handle IDs as the actual IOSC because there are ioctlvs
  // that accept arbitrary key handles from the PPC, so the IDs must match.
  // More information on default handles: https://wiibrew.org/wiki/IOS/Syscalls
  enum DefaultHandle : u32
  {
    // NG private key. ECC-233 private signing key (per-console)
    HANDLE_CONSOLE_KEY = 0,
    // Console ID
    HANDLE_CONSOLE_ID = 1,
    // NAND FS AES-128 key
    HANDLE_FS_KEY = 2,
    // NAND FS HMAC
    HANDLE_FS_MAC = 3,
    // Common key
    HANDLE_COMMON_KEY = 4,
    // PRNG seed
    HANDLE_PRNG_KEY = 5,
    // SD AES-128 key
    HANDLE_SD_KEY = 6,
    // boot2 version (writable)
    HANDLE_BOOT2_VERSION = 7,
    // Unknown
    HANDLE_UNKNOWN_8 = 8,
    // Unknown
    HANDLE_UNKNOWN_9 = 9,
    // Filesystem version (writable)
    HANDLE_FS_VERSION = 10,
    // New common key (aka Korean common key)
    HANDLE_NEW_COMMON_KEY = 11,

    HANDLE_ROOT_KEY = 0xfffffff,
  };

  static constexpr std::array<DefaultHandle, 2> COMMON_KEY_HANDLES = {HANDLE_COMMON_KEY,
                                                                      HANDLE_NEW_COMMON_KEY};

  enum ObjectType : u8
  {
    TYPE_SECRET_KEY = 0,
    TYPE_PUBLIC_KEY = 1,
    TYPE_DATA = 3,
  };

  enum class ObjectSubType : u8
  {
    AES128 = 0,
    MAC = 1,
    RSA2048 = 2,
    RSA4096 = 3,
    ECC233 = 4,
    Data = 5,
    Version = 6
  };

  IOSC(ConsoleType console_type = ConsoleType::Retail);
  ~IOSC();

  // Create an object for use with the other functions that operate on objects.
  ReturnCode CreateObject(Handle* handle, ObjectType type, ObjectSubType subtype, u32 pid);
  // Delete an object. Built-in objects cannot be deleted.
  ReturnCode DeleteObject(Handle handle, u32 pid);
  // Import a secret, encrypted key into dest_handle, which will be decrypted using decrypt_handle.
  ReturnCode ImportSecretKey(Handle dest_handle, Handle decrypt_handle, u8* iv,
                             const u8* encrypted_key, u32 pid);
  // Import a secret key that is already decrypted.
  ReturnCode ImportSecretKey(Handle dest_handle, const u8* decrypted_key, u32 pid);
  // Import a public key. public_key_exponent must be passed for RSA keys.
  ReturnCode ImportPublicKey(Handle dest_handle, const u8* public_key,
                             const u8* public_key_exponent, u32 pid);
  // Compute an AES key from an ECDH shared secret.
  ReturnCode ComputeSharedKey(Handle dest_handle, Handle private_handle, Handle public_handle,
                              u32 pid);

  // AES encrypt/decrypt.
  ReturnCode Encrypt(Handle key_handle, u8* iv, const u8* input, size_t size, u8* output,
                     u32 pid) const;
  ReturnCode Decrypt(Handle key_handle, u8* iv, const u8* input, size_t size, u8* output,
                     u32 pid) const;

  ReturnCode VerifyPublicKeySign(const std::array<u8, 20>& sha1, Handle signer_handle,
                                 const std::vector<u8>& signature, u32 pid) const;
  // Import a certificate (signed by the certificate in signer_handle) into dest_handle.
  ReturnCode ImportCertificate(const ES::CertReader& cert, Handle signer_handle, Handle dest_handle,
                               u32 pid);

  // Ownership
  ReturnCode GetOwnership(Handle handle, u32* owner) const;
  ReturnCode SetOwnership(Handle handle, u32 owner, u32 pid);

  bool IsUsingDefaultId() const;
  u32 GetDeviceId() const;
  CertECC GetDeviceCertificate() const;
  void Sign(u8* sig_out, u8* ap_cert_out, u64 title_id, const u8* data, u32 data_size) const;

  void DoState(PointerWrap& p);

private:
  struct KeyEntry
  {
    KeyEntry();
    KeyEntry(ObjectType type_, ObjectSubType subtype_, std::vector<u8>&& data_, u32 owner_mask_);
    KeyEntry(ObjectType type_, ObjectSubType subtype_, std::vector<u8>&& data_, u32 misc_data_,
             u32 owner_mask_);
    void DoState(PointerWrap& p);

    bool in_use = false;
    ObjectType type{};
    ObjectSubType subtype{};
    std::vector<u8> data;
    u32 misc_data = 0;
    u32 owner_mask = 0;
  };
  // The Wii's IOSC is limited to 32 entries, including 12 built-in entries.
  using KeyEntries = std::array<KeyEntry, 32>;

  enum class SearchMode
  {
    IncludeRootKey,
    ExcludeRootKey,
  };

  void LoadDefaultEntries();
  void LoadEntries();

  KeyEntries::iterator FindFreeEntry();
  KeyEntry* FindEntry(Handle handle);
  const KeyEntry* FindEntry(Handle handle, SearchMode mode = SearchMode::ExcludeRootKey) const;

  Handle GetHandleFromIterator(KeyEntries::iterator iterator) const;
  bool HasOwnership(Handle handle, u32 pid) const;
  bool IsDefaultHandle(Handle handle) const;
  ReturnCode DecryptEncrypt(Common::AES::Mode mode, Handle key_handle, u8* iv, const u8* input,
                            size_t size, u8* output, u32 pid) const;

  ConsoleType m_console_type{ConsoleType::Retail};
  KeyEntries m_key_entries;
  KeyEntry m_root_key_entry;
  Common::ec::Signature m_console_signature{};
  u32 m_ms_id = 0;
  u32 m_ca_id = 0;
  u32 m_console_key_id = 0;
};
}  // namespace HLE
}  // namespace IOS
