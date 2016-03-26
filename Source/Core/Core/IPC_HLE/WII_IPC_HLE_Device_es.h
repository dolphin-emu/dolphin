// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Core/IPC_HLE/WII_IPC_HLE_Device.h"

class PointerWrap;
namespace DiscIO
{
	class CNANDContentLoader;
	struct SNANDContent;
}

class CWII_IPC_HLE_Device_es : public IWII_IPC_HLE_Device
{
public:

	CWII_IPC_HLE_Device_es(u32 _DeviceID, const std::string& _rDeviceName);

	virtual ~CWII_IPC_HLE_Device_es();

	void LoadWAD(const std::string& _rContentFile);

	void OpenInternal();

	void DoState(PointerWrap& p) override;

	IPCCommandResult Open(u32 _CommandAddress, u32 _Mode) override;
	IPCCommandResult Close(u32 _CommandAddress, bool _bForce) override;

	IPCCommandResult IOCtlV(u32 _CommandAddress) override;

	static u32 ES_DIVerify(const std::vector<u8>& tmd);

	// This should only be cleared on power reset
	static std::string m_ContentFile;

private:
	enum
	{
		IOCTL_ES_ADDTICKET             = 0x01,
		IOCTL_ES_ADDTITLESTART         = 0x02,
		IOCTL_ES_ADDCONTENTSTART       = 0x03,
		IOCTL_ES_ADDCONTENTDATA        = 0x04,
		IOCTL_ES_ADDCONTENTFINISH      = 0x05,
		IOCTL_ES_ADDTITLEFINISH        = 0x06,
		IOCTL_ES_GETDEVICEID           = 0x07,
		IOCTL_ES_LAUNCH                = 0x08,
		IOCTL_ES_OPENCONTENT           = 0x09,
		IOCTL_ES_READCONTENT           = 0x0A,
		IOCTL_ES_CLOSECONTENT          = 0x0B,
		IOCTL_ES_GETOWNEDTITLECNT      = 0x0C,
		IOCTL_ES_GETOWNEDTITLES        = 0x0D,
		IOCTL_ES_GETTITLECNT           = 0x0E,
		IOCTL_ES_GETTITLES             = 0x0F,
		IOCTL_ES_GETTITLECONTENTSCNT   = 0x10,
		IOCTL_ES_GETTITLECONTENTS      = 0x11,
		IOCTL_ES_GETVIEWCNT            = 0x12,
		IOCTL_ES_GETVIEWS              = 0x13,
		IOCTL_ES_GETTMDVIEWCNT         = 0x14,
		IOCTL_ES_GETTMDVIEWS           = 0x15,
		IOCTL_ES_GETCONSUMPTION        = 0x16,
		IOCTL_ES_DELETETITLE           = 0x17,
		IOCTL_ES_DELETETICKET          = 0x18,
		// IOCTL_ES_DIGETTMDVIEWSIZE   = 0x19,
		// IOCTL_ES_DIGETTMDVIEW       = 0x1A,
		IOCTL_ES_DIGETTICKETVIEW       = 0x1B,
		IOCTL_ES_DIVERIFY              = 0x1C,
		IOCTL_ES_GETTITLEDIR           = 0x1D,
		IOCTL_ES_GETDEVICECERT         = 0x1E,
		IOCTL_ES_IMPORTBOOT            = 0x1F,
		IOCTL_ES_GETTITLEID            = 0x20,
		IOCTL_ES_SETUID                = 0x21,
		IOCTL_ES_DELETETITLECONTENT    = 0x22,
		IOCTL_ES_SEEKCONTENT           = 0x23,
		IOCTL_ES_OPENTITLECONTENT      = 0x24,
		// IOCTL_ES_LAUNCHBC           = 0x25,
		// IOCTL_ES_EXPORTTITLEINIT    = 0x26,
		// IOCTL_ES_EXPORTCONTENTBEGIN = 0x27,
		// IOCTL_ES_EXPORTCONTENTDATA  = 0x28,
		// IOCTL_ES_EXPORTCONTENTEND   = 0x29,
		// IOCTL_ES_EXPORTTITLEDONE    = 0x2A,
		IOCTL_ES_ADDTMD                = 0x2B,
		IOCTL_ES_ENCRYPT               = 0x2C,
		IOCTL_ES_DECRYPT               = 0x2D,
		IOCTL_ES_GETBOOT2VERSION       = 0x2E,
		IOCTL_ES_ADDTITLECANCEL        = 0x2F,
		IOCTL_ES_SIGN                  = 0x30,
		// IOCTL_ES_VERIFYSIGN         = 0x31,
		IOCTL_ES_GETSTOREDCONTENTCNT   = 0x32,
		IOCTL_ES_GETSTOREDCONTENTS     = 0x33,
		IOCTL_ES_GETSTOREDTMDSIZE      = 0x34,
		IOCTL_ES_GETSTOREDTMD          = 0x35,
		IOCTL_ES_GETSHAREDCONTENTCNT   = 0x36,
		IOCTL_ES_GETSHAREDCONTENTS     = 0x37,
		IOCTL_ES_DELETESHAREDCONTENT   = 0x38,
		//
		IOCTL_ES_CHECKKOREAREGION      = 0x45,
	};

	enum EErrorCodes
	{
		ES_INVALID_TMD                       = -106,   // or access denied
		ES_READ_LESS_DATA_THAN_EXPECTED      = -1009,
		ES_WRITE_FAILURE                     = -1010,
		ES_PARAMTER_SIZE_OR_ALIGNMENT        = -1017,
		ES_HASH_DOESNT_MATCH                 = -1022,
		ES_MEM_ALLOC_FAILED                  = -1024,
		ES_INCORRECT_ACCESS_RIGHT            = -1026,
		ES_NO_TICKET_INSTALLED               = -1028,
		ES_INSTALLED_TICKET_INVALID          = -1029,
		ES_INVALID_PARAMETR                  = -2008,
		ES_SIGNATURE_CHECK_FAILED            = -2011,
		ES_HASH_SIZE_WRONG                   = -2014, // HASH !=20
	};

	struct SContentAccess
	{
		u32 m_Position;
		u64 m_TitleID;
		u16 m_Index;
		u32 m_Size;
	};

	typedef std::map<u32, SContentAccess> CContentAccessMap;
	CContentAccessMap m_ContentAccessMap;

	std::vector<u64> m_TitleIDs;
	u64 m_TitleID;
	u32 m_AccessIdentID;

	static u8 *keyTable[11];

	const DiscIO::CNANDContentLoader& AccessContentDevice(u64 title_id);
	u32 OpenTitleContent(u32 CFD, u64 TitleID, u16 Index);

	struct ecc_cert_t
	{
		u32 sig_type          ;
		 u8 sig         [0x3c];
		 u8 pad         [0x40];
		 u8 issuer      [0x40];
		u32 key_type          ;
		 u8 key_name    [0x40];
		u32 ng_key_id         ;
		 u8 ecc_pubkey  [0x3c];
		 u8 padding     [0x3c];
	};
};
