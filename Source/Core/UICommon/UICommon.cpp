// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
#include "Core/HW/Wiimote.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/VideoBackendBase.h"

namespace UICommon
{

void Init()
{
	LogManager::Init();
	SConfig::Init();
	VideoBackend::PopulateList();
	WiimoteReal::LoadSettings();

	VideoBackend::ActivateBackend(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend);

	SetEnableAlert(SConfig::GetInstance().m_LocalCoreStartupParameter.bUsePanicHandlers);
}

void Shutdown()
{
	WiimoteReal::Shutdown();
	VideoBackend::ClearList();
	SConfig::Shutdown();
	LogManager::Shutdown();
}

void CreateDirectories()
{
	// Copy initial Wii NAND data from Sys to User.
	File::CopyDir(File::GetSysDirectory() + WII_USER_DIR DIR_SEP,
				  File::GetUserPath(D_WIIUSER_IDX));

	File::CreateFullPath(File::GetUserPath(D_USER_IDX));
	File::CreateFullPath(File::GetUserPath(D_CACHE_IDX));
	File::CreateFullPath(File::GetUserPath(D_CONFIG_IDX));
	File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
	File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
	File::CreateFullPath(File::GetUserPath(D_GAMESETTINGS_IDX));
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX));
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + USA_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + EUR_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + JAP_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_HIRESTEXTURES_IDX));
	File::CreateFullPath(File::GetUserPath(D_MAILLOGS_IDX));
	File::CreateFullPath(File::GetUserPath(D_MAPS_IDX));
	File::CreateFullPath(File::GetUserPath(D_SCREENSHOTS_IDX));
	File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX));
	File::CreateFullPath(File::GetUserPath(D_STATESAVES_IDX));
	File::CreateFullPath(File::GetUserPath(D_THEMES_IDX));
}

} // namespace UICommon
