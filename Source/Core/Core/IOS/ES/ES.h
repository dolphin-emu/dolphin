// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/IOS/Device.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOS.h"
#include "Core/IOS/IOSC.h"

class PointerWrap;

namespace DiscIO
{
class NANDContentLoader;
}

namespace IOS
{
namespace HLE
{
namespace Device
{
struct TitleContext
{
  void Clear();
  void DoState(PointerWrap& p);
  void Update(const DiscIO::NANDContentLoader& content_loader);
  void Update(const IOS::ES::TMDReader& tmd_, const IOS::ES::TicketReader& ticket_);

  IOS::ES::TicketReader ticket;
  IOS::ES::TMDReader tmd;
  bool active = false;
  bool first_change = true;
};

class ES final : public Device
{
public:
  ES(Kernel& ios, const std::string& device_name);

  static s32 DIVerify(const IOS::ES::TMDReader& tmd, const IOS::ES::TicketReader& ticket);
  static void LoadWAD(const std::string& _rContentFile);
  bool LaunchTitle(u64 title_id, bool skip_reload = false);

  void DoState(PointerWrap& p) override;

  ReturnCode Open(const OpenRequest& request) override;
  ReturnCode Close(u32 fd) override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

  struct TitleImportExportContext
  {
    void DoState(PointerWrap& p);

    bool valid = false;
    IOS::ES::TMDReader tmd;
    IOSC::Handle key_handle = 0;
    struct ContentContext
    {
      bool valid = false;
      u32 id = 0;
      std::array<u8, 16> iv{};
      std::vector<u8> buffer;
    };
    ContentContext content;
  };

  struct Context
  {
    void DoState(PointerWrap& p);

    u16 gid = 0;
    u32 uid = 0;
    TitleImportExportContext title_import_export;
    bool active = false;
    // We use this to associate an IPC fd with an ES context.
    s32 ipc_fd = -1;
  };

  IOS::ES::TMDReader FindImportTMD(u64 title_id) const;
  IOS::ES::TMDReader FindInstalledTMD(u64 title_id) const;

  // Get installed titles (in /title) without checking for TMDs at all.
  std::vector<u64> GetInstalledTitles() const;
  // Get titles which are being imported (in /import) without checking for TMDs at all.
  std::vector<u64> GetTitleImports() const;
  // Get titles for which there is a ticket (in /ticket).
  std::vector<u64> GetTitlesWithTickets() const;

  std::vector<IOS::ES::Content> GetStoredContentsFromTMD(const IOS::ES::TMDReader& tmd) const;
  u32 GetSharedContentsCount() const;
  std::vector<std::array<u8, 20>> GetSharedContents() const;

  // Title contents
  s32 OpenContent(const IOS::ES::TMDReader& tmd, u16 content_index, u32 uid);
  ReturnCode CloseContent(u32 cfd, u32 uid);
  s32 ReadContent(u32 cfd, u8* buffer, u32 size, u32 uid);
  s32 SeekContent(u32 cfd, u32 offset, SeekMode mode, u32 uid);

  // Title management
  ReturnCode ImportTicket(const std::vector<u8>& ticket_bytes, const std::vector<u8>& cert_chain);
  ReturnCode ImportTmd(Context& context, const std::vector<u8>& tmd_bytes);
  ReturnCode ImportTitleInit(Context& context, const std::vector<u8>& tmd_bytes,
                             const std::vector<u8>& cert_chain);
  ReturnCode ImportContentBegin(Context& context, u64 title_id, u32 content_id);
  ReturnCode ImportContentData(Context& context, u32 content_fd, const u8* data, u32 data_size);
  ReturnCode ImportContentEnd(Context& context, u32 content_fd);
  ReturnCode ImportTitleDone(Context& context);
  ReturnCode ImportTitleCancel(Context& context);
  ReturnCode ExportTitleInit(Context& context, u64 title_id, u8* tmd, u32 tmd_size);
  ReturnCode ExportContentBegin(Context& context, u64 title_id, u32 content_id);
  ReturnCode ExportContentData(Context& context, u32 content_fd, u8* data, u32 data_size);
  ReturnCode ExportContentEnd(Context& context, u32 content_fd);
  ReturnCode ExportTitleDone(Context& context);
  ReturnCode DeleteTitle(u64 title_id);
  ReturnCode DeleteTitleContent(u64 title_id) const;
  ReturnCode DeleteTicket(const u8* ticket_view);
  ReturnCode DeleteSharedContent(const std::array<u8, 20>& sha1) const;
  ReturnCode DeleteContent(u64 title_id, u32 content_id) const;

  ReturnCode GetDeviceId(u32* device_id) const;
  ReturnCode GetTitleId(u64* device_id) const;

  // Views
  ReturnCode GetV0TicketFromView(const u8* ticket_view, u8* ticket) const;
  ReturnCode GetTicketFromView(const u8* ticket_view, u8* ticket, u32* ticket_size) const;

  ReturnCode SetUpStreamKey(u32 uid, const u8* ticket_view, const IOS::ES::TMDReader& tmd,
                            u32* handle);

private:
  enum
  {
    IOCTL_ES_ADDTICKET = 0x01,
    IOCTL_ES_ADDTITLESTART = 0x02,
    IOCTL_ES_ADDCONTENTSTART = 0x03,
    IOCTL_ES_ADDCONTENTDATA = 0x04,
    IOCTL_ES_ADDCONTENTFINISH = 0x05,
    IOCTL_ES_ADDTITLEFINISH = 0x06,
    IOCTL_ES_GETDEVICEID = 0x07,
    IOCTL_ES_LAUNCH = 0x08,
    IOCTL_ES_OPEN_ACTIVE_TITLE_CONTENT = 0x09,
    IOCTL_ES_READCONTENT = 0x0A,
    IOCTL_ES_CLOSECONTENT = 0x0B,
    IOCTL_ES_GETOWNEDTITLECNT = 0x0C,
    IOCTL_ES_GETOWNEDTITLES = 0x0D,
    IOCTL_ES_GETTITLECNT = 0x0E,
    IOCTL_ES_GETTITLES = 0x0F,
    IOCTL_ES_GETTITLECONTENTSCNT = 0x10,
    IOCTL_ES_GETTITLECONTENTS = 0x11,
    IOCTL_ES_GETVIEWCNT = 0x12,
    IOCTL_ES_GETVIEWS = 0x13,
    IOCTL_ES_GETTMDVIEWCNT = 0x14,
    IOCTL_ES_GETTMDVIEWS = 0x15,
    IOCTL_ES_GETCONSUMPTION = 0x16,
    IOCTL_ES_DELETETITLE = 0x17,
    IOCTL_ES_DELETETICKET = 0x18,
    IOCTL_ES_DIGETTMDVIEWSIZE = 0x19,
    IOCTL_ES_DIGETTMDVIEW = 0x1A,
    IOCTL_ES_DIGETTICKETVIEW = 0x1B,
    IOCTL_ES_DIVERIFY = 0x1C,
    IOCTL_ES_GETTITLEDIR = 0x1D,
    IOCTL_ES_GETDEVICECERT = 0x1E,
    IOCTL_ES_IMPORTBOOT = 0x1F,
    IOCTL_ES_GETTITLEID = 0x20,
    IOCTL_ES_SETUID = 0x21,
    IOCTL_ES_DELETETITLECONTENT = 0x22,
    IOCTL_ES_SEEKCONTENT = 0x23,
    IOCTL_ES_OPENCONTENT = 0x24,
    IOCTL_ES_LAUNCHBC = 0x25,
    IOCTL_ES_EXPORTTITLEINIT = 0x26,
    IOCTL_ES_EXPORTCONTENTBEGIN = 0x27,
    IOCTL_ES_EXPORTCONTENTDATA = 0x28,
    IOCTL_ES_EXPORTCONTENTEND = 0x29,
    IOCTL_ES_EXPORTTITLEDONE = 0x2A,
    IOCTL_ES_ADDTMD = 0x2B,
    IOCTL_ES_ENCRYPT = 0x2C,
    IOCTL_ES_DECRYPT = 0x2D,
    IOCTL_ES_GETBOOT2VERSION = 0x2E,
    IOCTL_ES_ADDTITLECANCEL = 0x2F,
    IOCTL_ES_SIGN = 0x30,
    IOCTL_ES_VERIFYSIGN = 0x31,
    IOCTL_ES_GETSTOREDCONTENTCNT = 0x32,
    IOCTL_ES_GETSTOREDCONTENTS = 0x33,
    IOCTL_ES_GETSTOREDTMDSIZE = 0x34,
    IOCTL_ES_GETSTOREDTMD = 0x35,
    IOCTL_ES_GETSHAREDCONTENTCNT = 0x36,
    IOCTL_ES_GETSHAREDCONTENTS = 0x37,
    IOCTL_ES_DELETESHAREDCONTENT = 0x38,
    IOCTL_ES_DIGETTMDSIZE = 0x39,
    IOCTL_ES_DIGETTMD = 0x3A,
    IOCTL_ES_DIVERIFY_WITH_VIEW = 0x3B,
    IOCTL_ES_SET_UP_STREAM_KEY = 0x3C,
    IOCTL_ES_DELETE_STREAM_KEY = 0x3D,
    IOCTL_ES_DELETE_CONTENT = 0x3E,
    IOCTL_ES_INVALID_3F = 0x3F,
    IOCTL_ES_GET_V0_TICKET_FROM_VIEW = 0x40,
    IOCTL_ES_UNKNOWN_41 = 0x41,
    IOCTL_ES_UNKNOWN_42 = 0x42,
    IOCTL_ES_GET_TICKET_SIZE_FROM_VIEW = 0x43,
    IOCTL_ES_GET_TICKET_FROM_VIEW = 0x44,
    IOCTL_ES_CHECKKOREAREGION = 0x45,
  };

  // ES can only have 3 contexts at one time.
  using ContextArray = std::array<Context, 3>;

  // Title management
  IPCCommandResult ImportTicket(const IOCtlVRequest& request);
  IPCCommandResult ImportTmd(Context& context, const IOCtlVRequest& request);
  IPCCommandResult ImportTitleInit(Context& context, const IOCtlVRequest& request);
  IPCCommandResult ImportContentBegin(Context& context, const IOCtlVRequest& request);
  IPCCommandResult ImportContentData(Context& context, const IOCtlVRequest& request);
  IPCCommandResult ImportContentEnd(Context& context, const IOCtlVRequest& request);
  IPCCommandResult ImportTitleDone(Context& context, const IOCtlVRequest& request);
  IPCCommandResult ImportTitleCancel(Context& context, const IOCtlVRequest& request);
  IPCCommandResult ExportTitleInit(Context& context, const IOCtlVRequest& request);
  IPCCommandResult ExportContentBegin(Context& context, const IOCtlVRequest& request);
  IPCCommandResult ExportContentData(Context& context, const IOCtlVRequest& request);
  IPCCommandResult ExportContentEnd(Context& context, const IOCtlVRequest& request);
  IPCCommandResult ExportTitleDone(Context& context, const IOCtlVRequest& request);
  IPCCommandResult DeleteTitle(const IOCtlVRequest& request);
  IPCCommandResult DeleteTitleContent(const IOCtlVRequest& request);
  IPCCommandResult DeleteTicket(const IOCtlVRequest& request);
  IPCCommandResult DeleteSharedContent(const IOCtlVRequest& request);
  IPCCommandResult DeleteContent(const IOCtlVRequest& request);

  // Device identity and encryption
  IPCCommandResult GetDeviceId(const IOCtlVRequest& request);
  IPCCommandResult GetDeviceCertificate(const IOCtlVRequest& request);
  IPCCommandResult CheckKoreaRegion(const IOCtlVRequest& request);
  IPCCommandResult Sign(const IOCtlVRequest& request);
  IPCCommandResult Encrypt(u32 uid, const IOCtlVRequest& request);
  IPCCommandResult Decrypt(u32 uid, const IOCtlVRequest& request);

  // Misc
  IPCCommandResult SetUID(u32 uid, const IOCtlVRequest& request);
  IPCCommandResult GetTitleDirectory(const IOCtlVRequest& request);
  IPCCommandResult GetTitleId(const IOCtlVRequest& request);
  IPCCommandResult GetConsumption(const IOCtlVRequest& request);
  IPCCommandResult Launch(const IOCtlVRequest& request);
  IPCCommandResult LaunchBC(const IOCtlVRequest& request);
  IPCCommandResult DIVerify(const IOCtlVRequest& request);
  IPCCommandResult SetUpStreamKey(const Context& context, const IOCtlVRequest& request);
  IPCCommandResult DeleteStreamKey(const IOCtlVRequest& request);

  // Title contents
  IPCCommandResult OpenActiveTitleContent(u32 uid, const IOCtlVRequest& request);
  IPCCommandResult OpenContent(u32 uid, const IOCtlVRequest& request);
  IPCCommandResult ReadContent(u32 uid, const IOCtlVRequest& request);
  IPCCommandResult CloseContent(u32 uid, const IOCtlVRequest& request);
  IPCCommandResult SeekContent(u32 uid, const IOCtlVRequest& request);

  // Title information
  IPCCommandResult GetTitleCount(const std::vector<u64>& titles, const IOCtlVRequest& request);
  IPCCommandResult GetTitles(const std::vector<u64>& titles, const IOCtlVRequest& request);
  IPCCommandResult GetOwnedTitleCount(const IOCtlVRequest& request);
  IPCCommandResult GetOwnedTitles(const IOCtlVRequest& request);
  IPCCommandResult GetTitleCount(const IOCtlVRequest& request);
  IPCCommandResult GetTitles(const IOCtlVRequest& request);
  IPCCommandResult GetBoot2Version(const IOCtlVRequest& request);
  IPCCommandResult GetStoredContentsCount(const IOS::ES::TMDReader& tmd,
                                          const IOCtlVRequest& request);
  IPCCommandResult GetStoredContents(const IOS::ES::TMDReader& tmd, const IOCtlVRequest& request);
  IPCCommandResult GetStoredContentsCount(const IOCtlVRequest& request);
  IPCCommandResult GetStoredContents(const IOCtlVRequest& request);
  IPCCommandResult GetTMDStoredContentsCount(const IOCtlVRequest& request);
  IPCCommandResult GetTMDStoredContents(const IOCtlVRequest& request);
  IPCCommandResult GetStoredTMDSize(const IOCtlVRequest& request);
  IPCCommandResult GetStoredTMD(const IOCtlVRequest& request);
  IPCCommandResult GetSharedContentsCount(const IOCtlVRequest& request) const;
  IPCCommandResult GetSharedContents(const IOCtlVRequest& request) const;

  // Views for tickets and TMDs
  IPCCommandResult GetTicketViewCount(const IOCtlVRequest& request);
  IPCCommandResult GetTicketViews(const IOCtlVRequest& request);
  IPCCommandResult GetV0TicketFromView(const IOCtlVRequest& request);
  IPCCommandResult GetTicketSizeFromView(const IOCtlVRequest& request);
  IPCCommandResult GetTicketFromView(const IOCtlVRequest& request);
  IPCCommandResult GetTMDViewSize(const IOCtlVRequest& request);
  IPCCommandResult GetTMDViews(const IOCtlVRequest& request);
  IPCCommandResult DIGetTicketView(const IOCtlVRequest& request);
  IPCCommandResult DIGetTMDViewSize(const IOCtlVRequest& request);
  IPCCommandResult DIGetTMDView(const IOCtlVRequest& request);
  IPCCommandResult DIGetTMDSize(const IOCtlVRequest& request);
  IPCCommandResult DIGetTMD(const IOCtlVRequest& request);

  ContextArray::iterator FindActiveContext(s32 fd);
  ContextArray::iterator FindInactiveContext();

  bool LaunchIOS(u64 ios_title_id);
  bool LaunchPPCTitle(u64 title_id, bool skip_reload);
  static TitleContext& GetTitleContext();
  bool IsActiveTitlePermittedByTicket(const u8* ticket_view) const;

  ReturnCode CheckStreamKeyPermissions(u32 uid, const u8* ticket_view,
                                       const IOS::ES::TMDReader& tmd) const;

  enum class VerifyContainerType
  {
    TMD,
    Ticket,
    Device,
  };
  enum class VerifyMode
  {
    // Whether or not new certificates should be added to the certificate store (/sys/cert.sys).
    DoNotUpdateCertStore,
    UpdateCertStore,
  };
  bool IsIssuerCorrect(VerifyContainerType type, const IOS::ES::CertReader& issuer_cert) const;
  ReturnCode ReadCertStore(std::vector<u8>* buffer) const;
  ReturnCode WriteNewCertToStore(const IOS::ES::CertReader& cert);
  ReturnCode VerifyContainer(VerifyContainerType type, VerifyMode mode,
                             const IOS::ES::SignedBlobReader& signed_blob,
                             const std::vector<u8>& cert_chain, u32 iosc_handle = 0);

  // Start a title import.
  bool InitImport(u64 title_id);
  // Clean up the import content directory and move it back to /title.
  bool FinishImport(const IOS::ES::TMDReader& tmd);
  // Write a TMD for a title in /import atomically.
  bool WriteImportTMD(const IOS::ES::TMDReader& tmd);
  // Finish stale imports and clear the import directory.
  void FinishStaleImport(u64 title_id);
  void FinishAllStaleImports();

  static const DiscIO::NANDContentLoader& AccessContentDevice(u64 title_id);

  // TODO: reuse the FS code.
  struct OpenedContent
  {
    bool m_opened = false;
    u64 m_title_id = 0;
    IOS::ES::Content m_content;
    u32 m_position = 0;
    u32 m_uid = 0;
  };

  using ContentTable = std::array<OpenedContent, 16>;
  ContentTable m_content_table;

  ContextArray m_contexts;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
