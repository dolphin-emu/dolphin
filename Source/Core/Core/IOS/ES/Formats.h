// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Utilities to manipulate files and formats from the Wii's ES module: tickets,
// TMD, and other title informations.

#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/NandPaths.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/IOSC.h"
#include "DiscIO/Enums.h"

class PointerWrap;

namespace IOS
{
namespace ES
{
enum class TitleType : u32
{
  System = 0x00000001,
  Game = 0x00010000,
  Channel = 0x00010001,
  SystemChannel = 0x00010002,
  GameWithChannel = 0x00010004,
  DLC = 0x00010005,
  HiddenChannel = 0x00010008,
};

bool IsTitleType(u64 title_id, TitleType title_type);
bool IsDiscTitle(u64 title_id);
bool IsChannel(u64 title_id);

enum TitleFlags : u32
{
  // All official titles have this flag set.
  TITLE_TYPE_DEFAULT = 0x1,
  // Unknown.
  TITLE_TYPE_0x4 = 0x4,
  // Used for DLC titles.
  TITLE_TYPE_DATA = 0x8,
  // Unknown.
  TITLE_TYPE_0x10 = 0x10,
  // Appears to be used for WFS titles.
  TITLE_TYPE_WFS_MAYBE = 0x20,
  // Unknown.
  TITLE_TYPE_CT = 0x40,
};

#pragma pack(push, 4)
struct TMDHeader
{
  SignatureRSA2048 signature;
  u8 tmd_version;
  u8 ca_crl_version;
  u8 signer_crl_version;
  u64 ios_id;
  u64 title_id;
  u32 title_flags;
  u16 group_id;
  u16 zero;
  u16 region;
  u8 ratings[16];
  u8 reserved[12];
  u8 ipc_mask[12];
  u8 reserved2[18];
  u32 access_rights;
  u16 title_version;
  u16 num_contents;
  u16 boot_index;
  u16 fill2;
};
static_assert(sizeof(TMDHeader) == 0x1e4, "TMDHeader has the wrong size");

struct Content
{
  bool IsShared() const;
  bool IsOptional() const;
  u32 id;
  u16 index;
  u16 type;
  u64 size;
  std::array<u8, 20> sha1;
};
static_assert(sizeof(Content) == 36, "Content has the wrong size");
bool operator==(const Content&, const Content&);
bool operator!=(const Content&, const Content&);

struct TimeLimit
{
  u32 enabled;
  u32 seconds;
};

struct TicketView
{
  u32 version;
  u64 ticket_id;
  u32 device_id;
  u64 title_id;
  u16 access_mask;
  u32 permitted_title_id;
  u32 permitted_title_mask;
  u8 title_export_allowed;
  u8 common_key_index;
  u8 unknown2[0x30];
  u8 content_access_permissions[0x40];
  TimeLimit time_limits[8];
};
static_assert(sizeof(TicketView) == 0xd8, "TicketView has the wrong size");

// This structure is used for (signed) tickets. Technically, there are other types of tickets
// (RSA4096, ECDSA, ...). However, only RSA2048 tickets have ever been seen and these are also
// the only ticket type that is supported by the Wii's IOS.
struct Ticket
{
  SignatureRSA2048 signature;
  u8 server_public_key[0x3c];
  u8 version;
  u8 ca_crl_version;
  u8 signer_crl_version;
  u8 title_key[0x10];
  u64 ticket_id;
  u32 device_id;
  u64 title_id;
  u16 access_mask;
  u16 ticket_version;
  u32 permitted_title_id;
  u32 permitted_title_mask;
  u8 title_export_allowed;
  u8 common_key_index;
  u8 unknown2[0x30];
  u8 content_access_permissions[0x40];
  TimeLimit time_limits[8];
};
static_assert(sizeof(Ticket) == 0x2A4, "Ticket has the wrong size");
#pragma pack(pop)

class SignedBlobReader
{
public:
  SignedBlobReader() = default;
  explicit SignedBlobReader(const std::vector<u8>& bytes);
  explicit SignedBlobReader(std::vector<u8>&& bytes);

  const std::vector<u8>& GetBytes() const;
  void SetBytes(const std::vector<u8>& bytes);
  void SetBytes(std::vector<u8>&& bytes);

  // Only checks whether the signature data could be parsed. The signature is not verified.
  bool IsSignatureValid() const;

  SignatureType GetSignatureType() const;
  std::vector<u8> GetSignatureData() const;
  size_t GetSignatureSize() const;
  // Returns the whole issuer chain.
  // Example: Root-CA00000001 if the blob was signed by CA00000001, which is signed by the Root.
  std::string GetIssuer() const;

  void DoState(PointerWrap& p);

protected:
  std::vector<u8> m_bytes;
};

bool IsValidTMDSize(size_t size);

class TMDReader final : public SignedBlobReader
{
public:
  TMDReader() = default;
  explicit TMDReader(const std::vector<u8>& bytes);
  explicit TMDReader(std::vector<u8>&& bytes);

  bool IsValid() const;

  // Returns parts of the TMD without any kind of parsing. Intended for use by ES.
  std::vector<u8> GetRawView() const;

  u16 GetBootIndex() const;
  u64 GetIOSId() const;
  u64 GetTitleId() const;
  u32 GetTitleFlags() const;
  u16 GetTitleVersion() const;
  u16 GetGroupId() const;

  // Provides a best guess for the region. Might be inaccurate or UNKNOWN_REGION.
  DiscIO::Region GetRegion() const;

  // Constructs a 6-character game ID in the format typically used by Dolphin.
  // If the 6-character game ID would contain unprintable characters,
  // the title ID converted to hexadecimal is returned instead.
  std::string GetGameID() const;

  u16 GetNumContents() const;
  bool GetContent(u16 index, Content* content) const;
  std::vector<Content> GetContents() const;
  bool FindContentById(u32 id, Content* content) const;
};

class TicketReader final : public SignedBlobReader
{
public:
  TicketReader() = default;
  explicit TicketReader(const std::vector<u8>& bytes);
  explicit TicketReader(std::vector<u8>&& bytes);

  bool IsValid() const;

  std::vector<u8> GetRawTicket(u64 ticket_id) const;
  size_t GetNumberOfTickets() const;

  // Returns a "raw" ticket view, without byte swapping. Intended for use from ES.
  // Theoretically, a ticket file can contain one or more tickets. In practice, most (all?)
  // official titles only have one ticket, but IOS *does* have code to handle ticket files with
  // more than just one ticket and generate ticket views for them, so we implement it too.
  std::vector<u8> GetRawTicketView(u32 ticket_num) const;

  u32 GetDeviceId() const;
  u64 GetTitleId() const;
  // Get the decrypted title key.
  std::array<u8, 16> GetTitleKey(const HLE::IOSC& iosc) const;
  // Same as the above version, but guesses the console type depending on the issuer
  // and constructs a temporary IOSC instance.
  std::array<u8, 16> GetTitleKey() const;

  // Deletes a ticket with the given ticket ID from the internal buffer.
  void DeleteTicket(u64 ticket_id);

  // Decrypts the title key field for a "personalised" ticket -- one that is device-specific
  // and has a title key that must be decrypted first.
  HLE::ReturnCode Unpersonalise(HLE::IOSC& iosc);

  // Reset the common key field back to 0 if it's an incorrect value.
  // Intended for use before importing fakesigned tickets, which tend to have a high bogus index.
  void FixCommonKeyIndex();
};

class SharedContentMap final
{
public:
  explicit SharedContentMap(Common::FromWhichRoot root);
  ~SharedContentMap();

  std::optional<std::string> GetFilenameFromSHA1(const std::array<u8, 20>& sha1) const;
  std::string AddSharedContent(const std::array<u8, 20>& sha1);
  bool DeleteSharedContent(const std::array<u8, 20>& sha1);
  std::vector<std::array<u8, 20>> GetHashes() const;

private:
  bool WriteEntries() const;

  struct Entry;
  Common::FromWhichRoot m_root;
  u32 m_last_id = 0;
  std::string m_file_path;
  std::vector<Entry> m_entries;
};

class UIDSys final
{
public:
  explicit UIDSys(Common::FromWhichRoot root);

  u32 GetUIDFromTitle(u64 title_id) const;
  u32 GetOrInsertUIDForTitle(u64 title_id);
  u32 GetNextUID() const;

private:
  std::string m_file_path;
  std::map<u32, u64> m_entries;
};

class CertReader final : public SignedBlobReader
{
public:
  explicit CertReader(std::vector<u8>&& bytes);

  bool IsValid() const;

  u32 GetId() const;
  // Returns the certificate name. Examples: XS00000003, CA00000001
  std::string GetName() const;
  PublicKeyType GetPublicKeyType() const;
  // Returns the public key bytes + any other data associated with it.
  // For RSA public keys, this includes 4 bytes for the exponent at the end.
  std::vector<u8> GetPublicKey() const;

private:
  bool m_is_valid = false;
};

std::map<std::string, CertReader> ParseCertChain(const std::vector<u8>& chain);
}  // namespace ES
}  // namespace IOS
