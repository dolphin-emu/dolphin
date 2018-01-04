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
#include "Core/IOS/IPC.h"

class PointerWrap;

namespace DiscIO
{
class CNANDContentLoader;
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
  void Update(const DiscIO::CNANDContentLoader& content_loader);
  void Update(const IOS::ES::TMDReader& tmd_, const IOS::ES::TicketReader& ticket_);

  IOS::ES::TicketReader ticket;
  IOS::ES::TMDReader tmd;
  bool active = false;
  bool first_change = true;
};

class ES final : public Device
{
public:
  ES(u32 device_id, const std::string& device_name);

  // Called after an IOS reload.
  static void Init();

  static s32 DIVerify(const IOS::ES::TMDReader& tmd, const IOS::ES::TicketReader& ticket);
  static void LoadWAD(const std::string& _rContentFile);
  static bool LaunchTitle(u64 title_id, bool skip_reload = false);

  // Internal implementation of the ES_DECRYPT ioctlv.
  static void DecryptContent(u32 key_index, u8* iv, u8* input, u32 size, u8* new_iv, u8* output);

  void DoState(PointerWrap& p) override;

  ReturnCode Open(const OpenRequest& request) override;
  void Close() override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

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
    IOCTL_ES_OPENCONTENT = 0x09,
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
    IOCTL_ES_OPENTITLECONTENT = 0x24,
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
    // IOCTL_ES_VERIFYSIGN         = 0x31,
    IOCTL_ES_GETSTOREDCONTENTCNT = 0x32,
    IOCTL_ES_GETSTOREDCONTENTS = 0x33,
    IOCTL_ES_GETSTOREDTMDSIZE = 0x34,
    IOCTL_ES_GETSTOREDTMD = 0x35,
    IOCTL_ES_GETSHAREDCONTENTCNT = 0x36,
    IOCTL_ES_GETSHAREDCONTENTS = 0x37,
    IOCTL_ES_DELETESHAREDCONTENT = 0x38,
    //
    IOCTL_ES_CHECKKOREAREGION = 0x45,
  };

  struct OpenedContent
  {
    u64 m_title_id;
    IOS::ES::Content m_content;
    u32 m_position;
  };

  struct ecc_cert_t
  {
    u32 sig_type;
    u8 sig[0x3c];
    u8 pad[0x40];
    u8 issuer[0x40];
    u32 key_type;
    u8 key_name[0x40];
    u32 ng_key_id;
    u8 ecc_pubkey[0x3c];
    u8 padding[0x3c];
  };

  // Title management
  IPCCommandResult AddTicket(const IOCtlVRequest& request);
  IPCCommandResult AddTMD(const IOCtlVRequest& request);
  IPCCommandResult AddTitleStart(const IOCtlVRequest& request);
  IPCCommandResult AddContentStart(const IOCtlVRequest& request);
  IPCCommandResult AddContentData(const IOCtlVRequest& request);
  IPCCommandResult AddContentFinish(const IOCtlVRequest& request);
  IPCCommandResult AddTitleFinish(const IOCtlVRequest& request);
  IPCCommandResult AddTitleCancel(const IOCtlVRequest& request);
  IPCCommandResult ExportTitleInit(const IOCtlVRequest& request);
  IPCCommandResult ExportContentBegin(const IOCtlVRequest& request);
  IPCCommandResult ExportContentData(const IOCtlVRequest& request);
  IPCCommandResult ExportContentEnd(const IOCtlVRequest& request);
  IPCCommandResult ExportTitleDone(const IOCtlVRequest& request);
  IPCCommandResult DeleteTitle(const IOCtlVRequest& request);
  IPCCommandResult DeleteTicket(const IOCtlVRequest& request);
  IPCCommandResult DeleteTitleContent(const IOCtlVRequest& request);

  // Device identity and encryption
  IPCCommandResult GetConsoleID(const IOCtlVRequest& request);
  IPCCommandResult GetDeviceCertificate(const IOCtlVRequest& request);
  IPCCommandResult CheckKoreaRegion(const IOCtlVRequest& request);
  IPCCommandResult Sign(const IOCtlVRequest& request);
  IPCCommandResult Encrypt(const IOCtlVRequest& request);
  IPCCommandResult Decrypt(const IOCtlVRequest& request);

  // Misc
  IPCCommandResult SetUID(const IOCtlVRequest& request);
  IPCCommandResult GetTitleDirectory(const IOCtlVRequest& request);
  IPCCommandResult GetTitleID(const IOCtlVRequest& request);
  IPCCommandResult GetConsumption(const IOCtlVRequest& request);
  IPCCommandResult Launch(const IOCtlVRequest& request);
  IPCCommandResult LaunchBC(const IOCtlVRequest& request);

  // Title contents
  IPCCommandResult OpenTitleContent(const IOCtlVRequest& request);
  IPCCommandResult OpenContent(const IOCtlVRequest& request);
  IPCCommandResult ReadContent(const IOCtlVRequest& request);
  IPCCommandResult CloseContent(const IOCtlVRequest& request);
  IPCCommandResult SeekContent(const IOCtlVRequest& request);

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

  // Views for tickets and TMDs
  IPCCommandResult GetTicketViewCount(const IOCtlVRequest& request);
  IPCCommandResult GetTicketViews(const IOCtlVRequest& request);
  IPCCommandResult GetTMDViewSize(const IOCtlVRequest& request);
  IPCCommandResult GetTMDViews(const IOCtlVRequest& request);
  IPCCommandResult DIGetTicketView(const IOCtlVRequest& request);
  IPCCommandResult DIGetTMDViewSize(const IOCtlVRequest& request);
  IPCCommandResult DIGetTMDView(const IOCtlVRequest& request);

  static bool LaunchIOS(u64 ios_title_id);
  static bool LaunchPPCTitle(u64 title_id, bool skip_reload);
  static TitleContext& GetTitleContext();

  static const DiscIO::CNANDContentLoader& AccessContentDevice(u64 title_id);

  u32 OpenTitleContent(u32 CFD, u64 TitleID, u16 Index);

  using ContentAccessMap = std::map<u32, OpenedContent>;
  ContentAccessMap m_ContentAccessMap;

  u32 m_AccessIdentID = 0;

  // For title installation (ioctls IOCTL_ES_ADDTITLE*).
  IOS::ES::TMDReader m_addtitle_tmd;
  u32 m_addtitle_content_id = 0xFFFFFFFF;
  std::vector<u8> m_addtitle_content_buffer;

  struct TitleExportContext
  {
    struct ExportContent
    {
      OpenedContent content;
      std::array<u8, 16> iv{};
    };

    bool valid = false;
    IOS::ES::TMDReader tmd;
    std::vector<u8> title_key;
    std::map<u32, ExportContent> contents;
  } m_export_title_context;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
