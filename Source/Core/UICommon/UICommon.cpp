// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef _WIN32
#include <shlobj.h>    // for SHGetFolderPath
#endif

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"

#include "Core/ConfigManager.h"
#if defined(__LIBUSB__) || defined (_WIN32)
#include "Core/HW/SI_GCAdapter.h"
#endif
#include "Core/HW/Wiimote.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VR.h"

namespace UICommon
{

void Init()
{
	LogManager::Init();
	SConfig::Init();
	VideoBackend::PopulateList();
	WiimoteReal::LoadSettings();
#if defined(__LIBUSB__) || defined (_WIN32)
	SI_GCAdapter::Init();
#endif
	VideoBackend::ActivateBackend(SConfig::GetInstance().m_strVideoBackend);

	SetEnableAlert(SConfig::GetInstance().bUsePanicHandlers);
}

void Shutdown()
{
#if defined(__LIBUSB__) || defined (_WIN32)
	SI_GCAdapter::Shutdown();
#endif
	WiimoteReal::Shutdown();
	VideoBackend::ClearList();
	SConfig::Shutdown();
	LogManager::Shutdown();
}

void CreateDirectories()
{
	// Copy initial Wii NAND data from Sys to User.
	File::CopyDir(File::GetSysDirectory() + WII_USER_DIR,
				  File::GetUserPath(D_WIIROOT_IDX));

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
	File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX) + ANAGLYPH_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_STATESAVES_IDX));
	File::CreateFullPath(File::GetUserPath(D_THEMES_IDX));
}

void SetUserDirectory(const std::string& custom_path)
{
	if (!custom_path.empty())
	{
		File::CreateFullPath(custom_path + DIR_SEP);
		File::SetUserPath(D_USER_IDX, custom_path + DIR_SEP);
		return;
	}

	std::string user_path = "";
#ifdef _WIN32
	// Detect where the User directory is. There are five different cases (on top of the
	// command line flag, which overrides all this):
	// 1. GetExeDirectory()\portable.txt exists
	//    -> Use GetExeDirectory()\User
	// 2. HKCU\Software\Dolphin Emulator\LocalUserConfig exists and is true
	//    -> Use GetExeDirectory()\User
	// 3. HKCU\Software\Dolphin Emulator\UserConfigPath exists
	//    -> Use this as the user directory path
	// 4. My Documents exists
	//    -> Use My Documents\Dolphin Emulator as the User directory path
	// 5. Default
	//    -> Use GetExeDirectory()\User

	// Check our registry keys
	HKEY hkey;
	DWORD local = 0;
	TCHAR configPath[MAX_PATH] = {0};
	if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\Dolphin Emulator"), 0, KEY_QUERY_VALUE, &hkey) == ERROR_SUCCESS)
	{
		DWORD size = 4;
		if (RegQueryValueEx(hkey, TEXT("LocalUserConfig"), nullptr, nullptr, reinterpret_cast<LPBYTE>(&local), &size) != ERROR_SUCCESS)
			local = 0;

		size = MAX_PATH;
		if (RegQueryValueEx(hkey, TEXT("UserConfigPath"), nullptr, nullptr, (LPBYTE)configPath, &size) != ERROR_SUCCESS)
			configPath[0] = 0;
		RegCloseKey(hkey);
	}

	local = local || File::Exists(File::GetExeDirectory() + DIR_SEP "portable.txt");

	// Get Program Files path in case we need it.
	TCHAR my_documents[MAX_PATH];
	bool my_documents_found = SUCCEEDED(SHGetFolderPath(nullptr, CSIDL_MYDOCUMENTS, nullptr, SHGFP_TYPE_CURRENT, my_documents));

	if (local) // Case 1-2
		user_path = File::GetExeDirectory() + DIR_SEP USERDATA_DIR DIR_SEP;
	else if (configPath[0]) // Case 3
		user_path = TStrToUTF8(configPath);
	else if (my_documents_found) // Case 4
		user_path = TStrToUTF8(my_documents) + DIR_SEP "Dolphin Emulator" DIR_SEP;
	else // Case 5
		user_path = File::GetExeDirectory() + DIR_SEP USERDATA_DIR DIR_SEP;

	// Prettify the path: it will be displayed in some places, we don't want a mix of \ and /.
	user_path = ReplaceAll(user_path, "\\", DIR_SEP);

	// Make sure it ends in DIR_SEP.
	if (*user_path.rbegin() != DIR_SEP_CHR)
		user_path += DIR_SEP;
#else
	if (File::Exists(ROOT_DIR DIR_SEP USERDATA_DIR))
		user_path = ROOT_DIR DIR_SEP USERDATA_DIR DIR_SEP;
	else
		user_path = std::string(getenv("HOME") ?
			getenv("HOME") : getenv("PWD") ?
			getenv("PWD") : "") + DIR_SEP DOLPHIN_DATA_DIR DIR_SEP;
#endif
	File::SetUserPath(D_USER_IDX, user_path);
}

} // namespace UICommon
