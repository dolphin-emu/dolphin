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
IOS::ES::TicketReader FindSignedTicket(u64 title_id);

class NANDContentData
{
public:
  virtual ~NANDContentData() = 0;
  virtual void Open() {}
  virtual std::vector<u8> Get() = 0;
  virtual bool GetRange(u32 start, u32 size, u8* buffer) = 0;
  virtual void Close() {}
};

class NANDContentDataFile final : public NANDContentData
{
public:
  explicit NANDContentDataFile(const std::string& filename);
  ~NANDContentDataFile();

  void Open() override;
  std::vector<u8> Get() override;
  bool GetRange(u32 start, u32 size, u8* buffer) override;
  void Close() override;

private:
  void EnsureOpen();

  const std::string m_filename;
  std::unique_ptr<File::IOFile> m_file;
};

class NANDContentDataBuffer final : public NANDContentData
{
public:
  explicit NANDContentDataBuffer(const std::vector<u8>& buffer) : m_buffer(buffer) {}
  std::vector<u8> Get() override { return m_buffer; }
  bool GetRange(u32 start, u32 size, u8* buffer) override;

private:
  const std::vector<u8> m_buffer;
};

struct NANDContent
{
  IOS::ES::Content m_metadata;
  std::unique_ptr<NANDContentData> m_Data;
};

// Instances of this class must be created by NANDContentManager
class NANDContentLoader final
{
public:
  explicit NANDContentLoader(const std::string& content_name, Common::FromWhichRoot from);
  ~NANDContentLoader();

  bool IsValid() const;
  const NANDContent* GetContentByID(u32 id) const;
  const NANDContent* GetContentByIndex(int index) const;
  const IOS::ES::TMDReader& GetTMD() const { return m_tmd; }
  const IOS::ES::TicketReader& GetTicket() const { return m_ticket; }
  const std::vector<NANDContent>& GetContent() const { return m_Content; }
private:
  bool Initialize(const std::string& name);
  void InitializeContentEntries(const std::vector<u8>& data_app);

  bool m_Valid = false;
  bool m_IsWAD = false;
  Common::FromWhichRoot m_root;
  std::string m_Path;
  IOS::ES::TMDReader m_tmd;
  IOS::ES::TicketReader m_ticket;

  std::vector<NANDContent> m_Content;
};

// we open the NAND Content files too often... let's cache them
class NANDContentManager
{
public:
  static NANDContentManager& Access()
  {
    static NANDContentManager instance;
    return instance;
  }

  const NANDContentLoader& GetNANDLoader(const std::string& content_path,
                                         Common::FromWhichRoot from = Common::FROM_CONFIGURED_ROOT);
  const NANDContentLoader& GetNANDLoader(u64 title_id,
                                         Common::FromWhichRoot from = Common::FROM_CONFIGURED_ROOT);
  void ClearCache();

private:
  NANDContentManager() {}
  ~NANDContentManager();

  NANDContentManager(NANDContentManager const&) = delete;
  void operator=(NANDContentManager const&) = delete;

  std::unordered_map<std::string, std::unique_ptr<NANDContentLoader>> m_map;
};
}
