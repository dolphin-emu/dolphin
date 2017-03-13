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
#include "Core/IOS/ES/Formats.h"

namespace File
{
class IOFile;
}

namespace DiscIO
{
enum class Region;

// TODO: move some of these to Core/IOS/ES.
bool AddTicket(const IOS::ES::TicketReader& signed_ticket);
IOS::ES::TicketReader FindSignedTicket(u64 title_id);

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
  IOS::ES::Content m_metadata;
  std::unique_ptr<CNANDContentData> m_Data;
};

// Instances of this class must be created by CNANDContentManager
class CNANDContentLoader final
{
public:
  explicit CNANDContentLoader(const std::string& content_name);
  ~CNANDContentLoader();

  bool IsValid() const;
  void RemoveTitle() const;
  const SNANDContent* GetContentByID(u32 id) const;
  const SNANDContent* GetContentByIndex(int index) const;
  const IOS::ES::TMDReader& GetTMD() const { return m_tmd; }
  const IOS::ES::TicketReader& GetTicket() const { return m_ticket; }
  const std::vector<SNANDContent>& GetContent() const { return m_Content; }
private:
  bool Initialize(const std::string& name);
  void InitializeContentEntries(const std::vector<u8>& data_app);

  bool m_Valid = false;
  bool m_IsWAD = false;
  std::string m_Path;
  IOS::ES::TMDReader m_tmd;
  IOS::ES::TicketReader m_ticket;

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
}
