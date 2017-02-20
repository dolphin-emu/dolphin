// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
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
struct SNANDContent;
}

namespace IOS
{
namespace HLE
{
namespace Device
{
class ES : public Device
{
public:
  ES(u32 device_id, const std::string& device_name);

  void LoadWAD(const std::string& _rContentFile);

  // Internal implementation of the ES_DECRYPT ioctlv.
  void DecryptContent(u32 key_index, u8* iv, u8* input, u32 size, u8* new_iv, u8* output);

  void OpenInternal();

  void DoState(PointerWrap& p) override;

  ReturnCode Open(const OpenRequest& request) override;
  void Close() override;
  IPCCommandResult IOCtlV(const IOCtlVRequest& request) override;

  static u32 ES_DIVerify(const std::vector<u8>& tmd);

  // This should only be cleared on power reset
  static std::string m_ContentFile;

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
    // IOCTL_ES_DIGETTMDVIEWSIZE   = 0x19,
    // IOCTL_ES_DIGETTMDVIEW       = 0x1A,
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
    // IOCTL_ES_EXPORTTITLEINIT    = 0x26,
    // IOCTL_ES_EXPORTCONTENTBEGIN = 0x27,
    // IOCTL_ES_EXPORTCONTENTDATA  = 0x28,
    // IOCTL_ES_EXPORTCONTENTEND   = 0x29,
    // IOCTL_ES_EXPORTTITLEDONE    = 0x2A,
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

  enum EErrorCodes
  {
    ES_INVALID_TMD = -106,  // or access denied
    ES_READ_LESS_DATA_THAN_EXPECTED = -1009,
    ES_WRITE_FAILURE = -1010,
    ES_PARAMETER_SIZE_OR_ALIGNMENT = -1017,
    ES_HASH_DOESNT_MATCH = -1022,
    ES_MEM_ALLOC_FAILED = -1024,
    ES_INCORRECT_ACCESS_RIGHT = -1026,
    ES_NO_TICKET_INSTALLED = -1028,
    ES_INSTALLED_TICKET_INVALID = -1029,
    ES_INVALID_PARAMETR = -2008,
    ES_SIGNATURE_CHECK_FAILED = -2011,
    ES_HASH_SIZE_WRONG = -2014,  // HASH !=20
  };

  struct SContentAccess
  {
    u32 m_Position;
    u64 m_TitleID;
    u16 m_Index;
    u32 m_Size;
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

  IPCCommandResult AddTicket(const IOCtlVRequest& request);
  IPCCommandResult AddTitleStart(const IOCtlVRequest& request);
  IPCCommandResult AddContentStart(const IOCtlVRequest& request);
  IPCCommandResult AddContentData(const IOCtlVRequest& request);
  IPCCommandResult AddContentFinish(const IOCtlVRequest& request);
  IPCCommandResult AddTitleFinish(const IOCtlVRequest& request);
  IPCCommandResult ESGetDeviceID(const IOCtlVRequest& request);
  IPCCommandResult GetTitleContentsCount(const IOCtlVRequest& request);
  IPCCommandResult GetTitleContents(const IOCtlVRequest& request);
  IPCCommandResult OpenTitleContent(const IOCtlVRequest& request);
  IPCCommandResult OpenContent(const IOCtlVRequest& request);
  IPCCommandResult ReadContent(const IOCtlVRequest& request);
  IPCCommandResult CloseContent(const IOCtlVRequest& request);
  IPCCommandResult SeekContent(const IOCtlVRequest& request);
  IPCCommandResult GetTitleDirectory(const IOCtlVRequest& request);
  IPCCommandResult GetTitleID(const IOCtlVRequest& request);
  IPCCommandResult SetUID(const IOCtlVRequest& request);
  IPCCommandResult GetTitleCount(const IOCtlVRequest& request);
  IPCCommandResult GetTitles(const IOCtlVRequest& request);
  IPCCommandResult GetViewCount(const IOCtlVRequest& request);
  IPCCommandResult GetViews(const IOCtlVRequest& request);
  IPCCommandResult GetTMDViewCount(const IOCtlVRequest& request);
  IPCCommandResult GetTMDViews(const IOCtlVRequest& request);
  IPCCommandResult GetConsumption(const IOCtlVRequest& request);
  IPCCommandResult DeleteTicket(const IOCtlVRequest& request);
  IPCCommandResult DeleteTitleContent(const IOCtlVRequest& request);
  IPCCommandResult GetStoredTMDSize(const IOCtlVRequest& request);
  IPCCommandResult GetStoredTMD(const IOCtlVRequest& request);
  IPCCommandResult Encrypt(const IOCtlVRequest& request);
  IPCCommandResult Decrypt(const IOCtlVRequest& request);
  IPCCommandResult Launch(const IOCtlVRequest& request);
  IPCCommandResult LaunchBC(const IOCtlVRequest& request);
  IPCCommandResult CheckKoreaRegion(const IOCtlVRequest& request);
  IPCCommandResult GetDeviceCertificate(const IOCtlVRequest& request);
  IPCCommandResult Sign(const IOCtlVRequest& request);
  IPCCommandResult GetBoot2Version(const IOCtlVRequest& request);
  IPCCommandResult DIGetTicketView(const IOCtlVRequest& request);
  IPCCommandResult GetOwnedTitleCount(const IOCtlVRequest& request);

  void ResetAfterLaunch(u64 ios_to_load) const;

  const DiscIO::CNANDContentLoader& AccessContentDevice(u64 title_id);
  u32 OpenTitleContent(u32 CFD, u64 TitleID, u16 Index);

  using CContentAccessMap = std::map<u32, SContentAccess>;
  CContentAccessMap m_ContentAccessMap;

  std::vector<u64> m_TitleIDs;
  u64 m_TitleID = -1;
  u32 m_AccessIdentID = 0;

  // For title installation (ioctls IOCTL_ES_ADDTITLE*).
  TMDReader m_addtitle_tmd;
  u32 m_addtitle_content_id = 0xFFFFFFFF;
  std::vector<u8> m_addtitle_content_buffer;
};
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
