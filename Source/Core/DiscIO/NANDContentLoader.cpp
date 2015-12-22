// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <mbedtls/aes.h>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

#include "DiscIO/NANDContentLoader.h"
#include "DiscIO/Volume.h"
#include "DiscIO/WiiWad.h"

namespace DiscIO
{

CSharedContent::CSharedContent()
{
	UpdateLocation();
}

void CSharedContent::UpdateLocation()
{
	m_Elements.clear();
	m_lastID = 0;
	m_contentMap = StringFromFormat("%s/shared1/content.map", File::GetUserPath(D_WIIROOT_IDX).c_str());

	File::IOFile pFile(m_contentMap, "rb");
	SElement Element;
	while (pFile.ReadArray(&Element, 1))
	{
		m_Elements.push_back(Element);
		m_lastID++;
	}
}

CSharedContent::~CSharedContent()
{}

std::string CSharedContent::GetFilenameFromSHA1(const u8* _pHash)
{
	for (auto& Element : m_Elements)
	{
		if (memcmp(_pHash, Element.SHA1Hash, 20) == 0)
		{
			return StringFromFormat("%s/shared1/%c%c%c%c%c%c%c%c.app", File::GetUserPath(D_WIIROOT_IDX).c_str(),
			    Element.FileName[0], Element.FileName[1], Element.FileName[2], Element.FileName[3],
			    Element.FileName[4], Element.FileName[5], Element.FileName[6], Element.FileName[7]);
		}
	}
	return "unk";
}

std::string CSharedContent::AddSharedContent(const u8* _pHash)
{
	std::string filename = GetFilenameFromSHA1(_pHash);

	if (strcasecmp(filename.c_str(), "unk") == 0)
	{
		std::string id = StringFromFormat("%08x", m_lastID);
		SElement Element;
		memcpy(Element.FileName, id.c_str(), 8);
		memcpy(Element.SHA1Hash, _pHash, 20);
		m_Elements.push_back(Element);

		File::CreateFullPath(m_contentMap);

		File::IOFile pFile(m_contentMap, "ab");
		pFile.WriteArray(&Element, 1);

		filename = StringFromFormat("%s/shared1/%s.app", File::GetUserPath(D_WIIROOT_IDX).c_str(), id.c_str());
		m_lastID++;
	}

	return filename;
}


CNANDContentLoader::CNANDContentLoader(const std::string& name)
	: m_Valid(false)
	, m_isWAD(false)
	, m_TitleID(-1)
	, m_IosVersion(0x09)
	, m_BootIndex(-1)
{
	m_Valid = Initialize(name);
}

CNANDContentLoader::~CNANDContentLoader()
{
}

const SNANDContent* CNANDContentLoader::GetContentByIndex(int index) const
{
	for (auto& Content : m_Content)
	{
		if (Content.m_Index == index)
		{
			return &Content;
		}
	}
	return nullptr;
}

bool CNANDContentLoader::Initialize(const std::string& name)
{
	if (name.empty())
		return false;

	m_Path = name;

	WiiWAD wad(name);
	std::vector<u8> data_app;
	std::vector<u8> tmd;
	std::vector<u8> decrypted_title_key;

	if (wad.IsValid())
	{
		m_isWAD = true;
		m_ticket = wad.GetTicket();
		decrypted_title_key = GetKeyFromTicket(m_ticket);
		tmd = wad.GetTMD();
		data_app = wad.GetDataApp();
	}
	else
	{
		std::string tmd_filename(m_Path);

		if (tmd_filename.back() == '/')
			tmd_filename += "title.tmd";
		else
			m_Path = tmd_filename.substr(0, tmd_filename.find("title.tmd"));

		File::IOFile tmd_file(tmd_filename, "rb");
		if (!tmd_file)
		{
			WARN_LOG(DISCIO, "CreateFromDirectory: error opening %s", tmd_filename.c_str());
			return false;
		}

		tmd.resize(static_cast<size_t>(File::GetSize(tmd_filename)));
		tmd_file.ReadBytes(tmd.data(), tmd.size());
	}

	std::copy(&tmd[0],     &tmd[TMD_HEADER_SIZE], m_TMDHeader);
	std::copy(&tmd[0x180], &tmd[0x180 + TMD_VIEW_SIZE], m_TMDView);

	m_TitleVersion = Common::swap16(&tmd[0x01DC]);
	m_numEntries   = Common::swap16(&tmd[0x01DE]);
	m_BootIndex    = Common::swap16(&tmd[0x01E0]);
	m_TitleID      = Common::swap64(&tmd[0x018C]);
	m_IosVersion   = Common::swap16(&tmd[0x018A]);
	m_Country      = static_cast<u8>(m_TitleID & 0xFF);

	if (m_Country == 2) // SYSMENU
		m_Country = GetSysMenuRegion(m_TitleVersion);

	InitializeContentEntries(tmd, decrypted_title_key, data_app);
	return true;
}

void CNANDContentLoader::InitializeContentEntries(const std::vector<u8>& tmd, const std::vector<u8>& decrypted_title_key, const std::vector<u8>& data_app)
{
	m_Content.resize(m_numEntries);

	std::array<u8, 16> iv;
	u32 data_app_offset = 0;

	for (u32 i = 0; i < m_numEntries; i++)
	{
		const u32 entry_offset = 0x24 * i;

		SNANDContent& content = m_Content[i];
		content.m_ContentID = Common::swap32(&tmd[entry_offset + 0x01E4]);
		content.m_Index     = Common::swap16(&tmd[entry_offset + 0x01E8]);
		content.m_Type      = Common::swap16(&tmd[entry_offset + 0x01EA]);
		content.m_Size      = static_cast<u32>(Common::swap64(&tmd[entry_offset + 0x01EC]));
		std::copy(&tmd[entry_offset + 0x01E4], &tmd[entry_offset + 0x01E4 + 36], content.m_Header);
		std::copy(&tmd[entry_offset + 0x01F4], &tmd[entry_offset + 0x01F4 + 20], content.m_SHA1Hash);

		if (m_isWAD)
		{
			u32 rounded_size = ROUND_UP(content.m_Size, 0x40);

			iv.fill(0);
			std::copy(&tmd[entry_offset + 0x01E8], &tmd[entry_offset + 0x01E8 + 2], iv.begin());

			content.m_data = AESDecode(decrypted_title_key.data(), iv.data(), &data_app[data_app_offset], rounded_size);

			data_app_offset += rounded_size;
			continue;
		}

		if (content.m_Type & 0x8000)  // shared app
			content.m_Filename = CSharedContent::AccessInstance().GetFilenameFromSHA1(content.m_SHA1Hash);
		else
			content.m_Filename = StringFromFormat("%s/%08x.app", m_Path.c_str(), content.m_ContentID);

		// Be graceful about incorrect TMDs.
		if (File::Exists(content.m_Filename))
			content.m_Size = static_cast<u32>(File::GetSize(content.m_Filename));
	}
}

std::vector<u8> CNANDContentLoader::AESDecode(const u8* key, u8* iv, const u8* src, u32 size)
{
	mbedtls_aes_context aes_ctx;
	std::vector<u8> buffer(size);

	mbedtls_aes_setkey_dec(&aes_ctx, key, 128);
	mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, size, iv, src, buffer.data());

	return buffer;
}

std::vector<u8> CNANDContentLoader::GetKeyFromTicket(const std::vector<u8>& ticket)
{
	const u8 common_key[16] = {0xeb,0xe4,0x2a,0x22,0x5e,0x85,0x93,0xe4,0x48,0xd9,0xc5,0x45,0x73,0x81,0xaa,0xf7};
	u8 iv[16] = {};

	std::copy(&ticket[0x01DC], &ticket[0x01DC + 8], iv);
	return AESDecode(common_key, iv, &ticket[0x01BF], 16);
}


DiscIO::IVolume::ECountry CNANDContentLoader::GetCountry() const
{
	if (!IsValid())
		return DiscIO::IVolume::COUNTRY_UNKNOWN;

	return CountrySwitch(m_Country);
}


CNANDContentManager::~CNANDContentManager()
{
}

const CNANDContentLoader& CNANDContentManager::GetNANDLoader(const std::string& content_path)
{
	auto it = m_map.find(content_path);
	if (it != m_map.end())
		return *it->second;
	return *m_map.emplace_hint(it, std::make_pair(content_path, std::make_unique<CNANDContentLoader>(content_path)))->second;
}

const CNANDContentLoader& CNANDContentManager::GetNANDLoader(u64 title_id, Common::FromWhichRoot from)
{
	std::string path = Common::GetTitleContentPath(title_id, from);
	return GetNANDLoader(path);
}

bool CNANDContentManager::RemoveTitle(u64 title_id, Common::FromWhichRoot from)
{
	auto& loader = GetNANDLoader(title_id, from);
	if (!loader.IsValid())
		return false;
	loader.RemoveTitle();
	return GetNANDLoader(title_id, from).IsValid();
}

void CNANDContentManager::ClearCache()
{
	m_map.clear();
}

void CNANDContentLoader::RemoveTitle() const
{
	INFO_LOG(DISCIO, "RemoveTitle %08x/%08x", (u32)(m_TitleID >> 32), (u32)m_TitleID);
	if (IsValid())
	{
		// remove TMD?
		for (u32 i = 0; i < m_numEntries; i++)
		{
			if (!(m_Content[i].m_Type & 0x8000)) // skip shared apps
			{
				std::string filename = StringFromFormat("%s/%08x.app", m_Path.c_str(), m_Content[i].m_ContentID);
				INFO_LOG(DISCIO, "Delete %s", filename.c_str());
				File::Delete(filename);
			}
		}
		CNANDContentManager::Access().ClearCache(); // deletes 'this'
	}
}

cUIDsys::cUIDsys()
{
	UpdateLocation();
}

void cUIDsys::UpdateLocation()
{
	m_Elements.clear();
	m_lastUID = 0x00001000;
	m_uidSys = File::GetUserPath(D_SESSION_WIIROOT_IDX) + "/sys/uid.sys";

	File::IOFile pFile(m_uidSys, "rb");
	SElement Element;
	while (pFile.ReadArray(&Element, 1))
	{
		*(u32*)&(Element.UID) = Common::swap32(m_lastUID++);
		m_Elements.push_back(Element);
	}
	pFile.Close();

	if (m_Elements.empty())
	{
		*(u64*)&(Element.titleID) = Common::swap64(TITLEID_SYSMENU);
		*(u32*)&(Element.UID) = Common::swap32(m_lastUID++);

		File::CreateFullPath(m_uidSys);
		pFile.Open(m_uidSys, "wb");
		if (!pFile.WriteArray(&Element, 1))
			ERROR_LOG(DISCIO, "Failed to write to %s", m_uidSys.c_str());
	}
}

cUIDsys::~cUIDsys()
{}

u32 cUIDsys::GetUIDFromTitle(u64 _Title)
{
	for (auto& Element : m_Elements)
	{
		if (Common::swap64(_Title) == *(u64*)&(Element.titleID))
		{
			return Common::swap32(Element.UID);
		}
	}
	return 0;
}

void cUIDsys::AddTitle(u64 _TitleID)
{
	if (GetUIDFromTitle(_TitleID))
	{
		INFO_LOG(DISCIO, "Title %08x%08x, already exists in uid.sys", (u32)(_TitleID >> 32), (u32)_TitleID);
		return;
	}

	SElement Element;
	*(u64*)&(Element.titleID) = Common::swap64(_TitleID);
	*(u32*)&(Element.UID) = Common::swap32(m_lastUID++);
	m_Elements.push_back(Element);

	File::CreateFullPath(m_uidSys);
	File::IOFile pFile(m_uidSys, "ab");

	if (!pFile.WriteArray(&Element, 1))
		ERROR_LOG(DISCIO, "fwrite failed");
}

void cUIDsys::GetTitleIDs(std::vector<u64>& _TitleIDs, bool _owned)
{
	for (auto& Element : m_Elements)
	{
		if ((_owned && Common::CheckTitleTIK(Common::swap64(Element.titleID), Common::FROM_SESSION_ROOT))  ||
			(!_owned && Common::CheckTitleTMD(Common::swap64(Element.titleID), Common::FROM_SESSION_ROOT)))
			_TitleIDs.push_back(Common::swap64(Element.titleID));
	}
}

u64 CNANDContentManager::Install_WiiWAD(const std::string& filename)
{
	if (filename.find(".wad") == std::string::npos)
		return 0;
	const CNANDContentLoader& content_loader = GetNANDLoader(filename);
	if (content_loader.IsValid() == false)
		return 0;

	u64 title_id = content_loader.GetTitleID();

	//copy WAD's TMD header and contents to content directory

	std::string content_path(Common::GetTitleContentPath(title_id, Common::FROM_CONFIGURED_ROOT));
	std::string tmd_filename(Common::GetTMDFileName(title_id, Common::FROM_CONFIGURED_ROOT));
	File::CreateFullPath(tmd_filename);

	File::IOFile tmd_file(tmd_filename, "wb");
	if (!tmd_file)
	{
		PanicAlertT("WAD installation failed: error creating %s", tmd_filename.c_str());
		return 0;
	}

	tmd_file.WriteBytes(content_loader.GetTMDHeader(), CNANDContentLoader::TMD_HEADER_SIZE);

	for (u32 i = 0; i < content_loader.GetContentSize(); i++)
	{
		const SNANDContent& content = content_loader.GetContent()[i];

		tmd_file.WriteBytes(content.m_Header, CNANDContentLoader::CONTENT_HEADER_SIZE);

		std::string app_filename;
		if (content.m_Type & 0x8000) //shared
			app_filename = CSharedContent::AccessInstance().AddSharedContent(content.m_SHA1Hash);
		else
			app_filename = StringFromFormat("%s%08x.app", content_path.c_str(), content.m_ContentID);

		if (!File::Exists(app_filename))
		{
			File::CreateFullPath(app_filename);
			File::IOFile app_file(app_filename, "wb");
			if (!app_file)
			{
				PanicAlertT("WAD installation failed: error creating %s", app_filename.c_str());
				return 0;
			}

			app_file.WriteBytes(content.m_data.data(), content.m_Size);
		}
		else
		{
			INFO_LOG(DISCIO, "Content %s already exists.", app_filename.c_str());
		}
	}

	//Extract and copy WAD's ticket to ticket directory
	if (!AddTicket(title_id, content_loader.GetTicket()))
	{
		PanicAlertT("WAD installation failed: error creating ticket");
		return 0;
	}

	cUIDsys::AccessInstance().AddTitle(title_id);

	ClearCache();

	return title_id;
}

bool AddTicket(u64 title_id, const std::vector<u8>& ticket)
{
	std::string ticket_filename = Common::GetTicketFileName(title_id, Common::FROM_CONFIGURED_ROOT);
	File::CreateFullPath(ticket_filename);

	File::IOFile ticket_file(ticket_filename, "wb");
	if (!ticket_file)
		return false;

	return ticket_file.WriteBytes(ticket.data(), ticket.size());
}

} // namespace end

