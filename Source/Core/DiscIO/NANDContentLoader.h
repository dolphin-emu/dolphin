// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/NandPaths.h"

namespace File
{
class IOFile;
}

namespace DiscIO
{
enum class Region;

bool AddTicket(const std::vector<u8>& signed_ticket);
std::vector<u8> FindSignedTicket(u64 title_id);
std::vector<u8> FindTicket(u64 title_id);
std::vector<u8> GetKeyFromTicket(const std::vector<u8>& ticket);

class CNANDContentData
{
public:
  virtual ~CNANDContentData() = 0;
  virtual void Open() {}
  virtual std::vector<u8> Get() = 0;
  virtual bool GetRange(u32 start, u32 size, u8* buffer) = 0;
  virtual void Close() {}
};

class CNANDContentDataFile final : public CNANDContentData
{
public:
  explicit CNANDContentDataFile(const std::string& filename);
  ~CNANDContentDataFile();

  void Open() override;
  std::vector<u8> Get() override;
  bool GetRange(u32 start, u32 size, u8* buffer) override;
  void Close() override;

private:
  void EnsureOpen();

  const std::string m_filename;
  std::unique_ptr<File::IOFile> m_file;
};
class CNANDContentDataBuffer final : public CNANDContentData
{
public:
  explicit CNANDContentDataBuffer(const std::vector<u8>& buffer) : m_buffer(buffer) {}
  std::vector<u8> Get() override { return m_buffer; }
  bool GetRange(u32 start, u32 size, u8* buffer) override;

private:
  const std::vector<u8> m_buffer;
};

struct SNANDContent
{
  u32 m_ContentID;
  u16 m_Index;
  u16 m_Type;
  u32 m_Size;
  u8 m_SHA1Hash[20];
  u8 m_Header[36];  // all of the above

  std::unique_ptr<CNANDContentData> m_Data;
};

// Instances of this class must be created by CNANDContentManager
class CNANDContentLoader final
{
public:
  explicit CNANDContentLoader(const std::string& content_name);
  ~CNANDContentLoader();

  bool IsValid() const { return m_Valid; }
  void RemoveTitle() const;
  u64 GetTitleID() const { return m_TitleID; }
  u16 GetIosVersion() const { return m_IosVersion; }
  u32 GetBootIndex() const { return m_BootIndex; }
  size_t GetContentSize() const { return m_Content.size(); }
  const SNANDContent* GetContentByIndex(int index) const;
  const u8* GetTMDView() const { return m_TMDView; }
  const u8* GetTMDHeader() const { return m_TMDHeader; }
  const std::vector<u8>& GetTicket() const { return m_Ticket; }
  const std::vector<SNANDContent>& GetContent() const { return m_Content; }
  u16 GetTitleVersion() const { return m_TitleVersion; }
  u16 GetNumEntries() const { return m_NumEntries; }
  DiscIO::Region GetRegion() const;
  u8 GetCountryChar() const { return m_Country; }
  enum
  {
    TMD_VIEW_SIZE = 0x58,
    TMD_HEADER_SIZE = 0x1E4,
    CONTENT_HEADER_SIZE = 0x24,
    TICKET_SIZE = 0x2A4
  };

private:
  bool Initialize(const std::string& name);
  void InitializeContentEntries(const std::vector<u8>& tmd,
                                const std::vector<u8>& decrypted_title_key,
                                const std::vector<u8>& data_app);

  bool m_Valid;
  bool m_IsWAD;
  std::string m_Path;
  u64 m_TitleID;
  u16 m_IosVersion;
  u32 m_BootIndex;
  u16 m_NumEntries;
  u16 m_TitleVersion;
  u8 m_TMDView[TMD_VIEW_SIZE];
  u8 m_TMDHeader[TMD_HEADER_SIZE];
  std::vector<u8> m_Ticket;
  u8 m_Country;

  std::vector<SNANDContent> m_Content;
};

// we open the NAND Content files too often... let's cache them
class CNANDContentManager
{
public:
  static CNANDContentManager& Access()
  {
    static CNANDContentManager instance;
    return instance;
  }
  u64 Install_WiiWAD(const std::string& fileName);

  const CNANDContentLoader& GetNANDLoader(const std::string& content_path);
  const CNANDContentLoader& GetNANDLoader(u64 title_id, Common::FromWhichRoot from);
  bool RemoveTitle(u64 title_id, Common::FromWhichRoot from);
  void ClearCache();

private:
  CNANDContentManager() {}
  ~CNANDContentManager();

  CNANDContentManager(CNANDContentManager const&) = delete;
  void operator=(CNANDContentManager const&) = delete;

  std::unordered_map<std::string, std::unique_ptr<CNANDContentLoader>> m_map;
};

class CSharedContent final
{
public:
  explicit CSharedContent(Common::FromWhichRoot root);

  std::string GetFilenameFromSHA1(const u8* hash) const;
  std::string AddSharedContent(const u8* hash);

private:
#pragma pack(push, 1)
  struct SElement
  {
    u8 FileName[8];
    u8 SHA1Hash[20];
  };
#pragma pack(pop)

  Common::FromWhichRoot m_root;
  u32 m_LastID;
  std::string m_ContentMap;
  std::vector<SElement> m_Elements;
};

class cUIDsys final
{
public:
  explicit cUIDsys(Common::FromWhichRoot root);

  u32 GetUIDFromTitle(u64 title_id);
  void AddTitle(u64 title_id);
  void GetTitleIDs(std::vector<u64>& title_ids, bool owned = false);

private:
#pragma pack(push, 1)
  struct SElement
  {
    u8 titleID[8];
    u8 UID[4];
  };
#pragma pack(pop)

  u32 m_LastUID;
  std::string m_UidSys;
  std::vector<SElement> m_Elements;
};
}
