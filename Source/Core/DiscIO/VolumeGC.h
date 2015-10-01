// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "DiscIO/Blob.h"
#include "DiscIO/Volume.h"

// --- this volume type is used for GC disc images ---

namespace DiscIO
{

class CVolumeGC : public IVolume
{
public:
	CVolumeGC(std::unique_ptr<IBlobReader> reader);
	~CVolumeGC();
	bool Read(u64 _Offset, u64 _Length, u8* _pBuffer, bool decrypt = false) const override;
	std::string GetUniqueID() const override;
	std::string GetMakerID() const override;
	u16 GetRevision() const override;
	std::string GetInternalName() const override;
	std::map<ELanguage, std::string> GetNames(bool prefer_long) const override;
	std::map<ELanguage, std::string> GetDescriptions() const override;
	std::string GetCompany() const override;
	std::vector<u32> GetBanner(int* width, int* height) const override;
	u64 GetFSTSize() const override;
	std::string GetApploaderDate() const override;
	u8 GetDiscNumber() const override;

	EPlatform GetVolumeType() const override;
	ECountry GetCountry() const override;
	BlobType GetBlobType() const override;
	u64 GetSize() const override;
	u64 GetRawSize() const override;

private:
	bool LoadBannerFile() const;
	std::map<ELanguage, std::string> ReadMultiLanguageStrings(bool description, bool prefer_long = true) const;

	static const int GC_BANNER_WIDTH = 96;
	static const int GC_BANNER_HEIGHT = 32;

	// Banner Comment
	struct GCBannerComment
	{
		char shortTitle[32]; // Short game title shown in IPL menu
		char shortMaker[32]; // Short developer, publisher names shown in IPL menu
		char longTitle[64];  // Long game title shown in IPL game start screen
		char longMaker[64];  // Long developer, publisher names shown in IPL game start screen
		char comment[128];   // Game description shown in IPL game start screen in two lines.
	};

	struct GCBanner
	{
		u32 id; // "BNR1" for NTSC, "BNR2" for PAL
		u32 padding[7];
		u16 image[GC_BANNER_WIDTH * GC_BANNER_HEIGHT]; // RGB5A3 96x32 image
		GCBannerComment comment[6]; // Comments in six languages (only one for BNR1 type)
	};

	static const size_t BNR1_SIZE = sizeof(GCBanner) - sizeof(GCBannerComment) * 5;
	static const size_t BNR2_SIZE = sizeof(GCBanner);

	enum BannerFileType
	{
		BANNER_NOT_LOADED,
		BANNER_INVALID,
		BANNER_BNR1,
		BANNER_BNR2
	};

	mutable BannerFileType m_banner_file_type = BANNER_NOT_LOADED;
	mutable GCBanner m_banner_file;

	std::unique_ptr<IBlobReader> m_pReader;
};

} // namespace
