// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official Git repository and contact information can be found at
// https://github.com/dolphin-emu/dolphin

#include <cstdio>
#include <cstdlib>
#include <jni.h>
#include <android/log.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>

#include "Android/ButtonManager.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/Logging/LogManager.h"
#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/State.h"
#include "Core/HW/Wiimote.h"
#include "Core/PowerPC/PowerPC.h"

#include "DiscIO/VolumeCreator.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoBackendBase.h"
#include "../DiscIO/Volume.h"

ANativeWindow* surf;
std::string g_filename;
std::string g_set_userpath = "";

#define DOLPHIN_TAG "DolphinEmuNative"

void Host_NotifyMapLoaded() {}
void Host_RefreshDSPDebuggerWindow() {}

Common::Event updateMainFrameEvent;
void Host_Message(int Id)
{
}

void* Host_GetRenderHandle()
{
	return surf;
}

void Host_UpdateTitle(const std::string& title)
{
	__android_log_write(ANDROID_LOG_INFO, DOLPHIN_TAG, title.c_str());
}

void Host_UpdateDisasmDialog(){}

void Host_UpdateMainFrame()
{
}

void Host_RequestRenderWindowSize(int width, int height) {}

void Host_RequestFullscreen(bool enable_fullscreen) {}

void Host_SetStartupDebuggingParameters()
{
}

bool Host_UIHasFocus()
{
	return true;
}

bool Host_RendererHasFocus()
{
	return true;
}

bool Host_RendererIsFullscreen()
{
	return false;
}

void Host_ConnectWiimote(int wm_idx, bool connect) {}

void Host_SetWiiMoteConnectionState(int _State) {}

void Host_ShowVideoConfig(void*, const std::string&, const std::string&) {}

static bool MsgAlert(const char* caption, const char* text, bool /*yes_no*/, int /*Style*/)
{
	__android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "%s:%s", caption, text);
	return false;
}

#define DVD_BANNER_WIDTH 96
#define DVD_BANNER_HEIGHT 32

static inline u32 Average32(u32 a, u32 b) {
	return ((a >> 1) & 0x7f7f7f7f) + ((b >> 1) & 0x7f7f7f7f);
}

static inline u32 GetPixel(u32 *buffer, unsigned int x, unsigned int y) {
	// thanks to unsignedness, these also check for <0 automatically.
	if (x > 191) return 0;
	if (y > 63) return 0;
	return buffer[y * 192 + x];
}

static bool LoadBanner(std::string filename, u32 *Banner)
{
	std::unique_ptr<DiscIO::IVolume> pVolume(DiscIO::CreateVolumeFromFilename(filename));

	if (pVolume != nullptr)
	{
		int Width, Height;
		std::vector<u32> BannerVec = pVolume->GetBanner(&Width, &Height);
		// This code (along with above inlines) is moved from
		// elsewhere.  Someone who knows anything about Android
		// please get rid of it and use proper high-resolution
		// images.
		if (Height == 64 && Width == 192)
		{
			u32* Buffer = &BannerVec[0];
			for (int y = 0; y < 32; y++)
			{
				for (int x = 0; x < 96; x++)
				{
					// simplified plus-shaped "gaussian"
					u32 surround = Average32(
							Average32(GetPixel(Buffer, x*2 - 1, y*2), GetPixel(Buffer, x*2 + 1, y*2)),
							Average32(GetPixel(Buffer, x*2, y*2 - 1), GetPixel(Buffer, x*2, y*2 + 1)));
					Banner[y * 96 + x] = Average32(GetPixel(Buffer, x*2, y*2), surround);
				}
			}
			return true;
		}
		else if (Height == 32 && Width == 96)
		{
			memcpy(Banner, &BannerVec[0], 96 * 32 * 4);
			return true;
		}
	}

	return false;
}

static bool IsWiiTitle(std::string filename)
{
	std::unique_ptr<DiscIO::IVolume> pVolume(DiscIO::CreateVolumeFromFilename(filename));

	if (pVolume != nullptr)
	{
		bool is_wii_title = pVolume->IsWiiDisc() || pVolume->IsWadFile();

		__android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Is %s a Wii Disc: %s", filename.c_str(), is_wii_title ? "Yes" : "No" );

		return is_wii_title;
	}

	// Technically correct.
	return false;
}

static int GetCountry(std::string filename)
{
	std::unique_ptr<DiscIO::IVolume> pVolume(DiscIO::CreateVolumeFromFilename(filename));

	if (pVolume != nullptr)
	{
		DiscIO::IVolume::ECountry country = pVolume->GetCountry();

		__android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Country Code: %i", country);

		return country;
	}

	// Technically correct.
	return 13;
}

static std::string GetTitle(std::string filename)
{
	__android_log_print(ANDROID_LOG_WARN, DOLPHIN_TAG, "Getting Title for file: %s", filename.c_str());

	std::unique_ptr<DiscIO::IVolume> pVolume(DiscIO::CreateVolumeFromFilename(filename));

	if (pVolume != nullptr) {
		std::map <DiscIO::IVolume::ELanguage, std::string> titles = pVolume->GetNames();


		/*bool is_wii_title = IsWiiTitle(filename);

		DiscIO::IVolume::ELanguage language = SConfig::GetInstance().m_LocalCoreStartupParameter.GetCurrentLanguage(
				is_wii_title);



		auto it = titles.find(language);
		if (it != end)
			return it->second;*/

		auto end = titles.end();

		// English tends to be a good fallback when the requested language isn't available
		//if (language != DiscIO::IVolume::ELanguage::LANGUAGE_ENGLISH) {
			auto it = titles.find(DiscIO::IVolume::ELanguage::LANGUAGE_ENGLISH);
			if (it != end)
				return it->second;
		//}


		// If English isn't available either, just pick something
		if (!titles.empty())
			return titles.cbegin()->second;

		// No usable name, return filename (better than nothing)
		std::string name;
		SplitPath(filename, nullptr, &name, nullptr);
		return name;
	}

	return std::string ("");
}

static std::string GetDescription(std::string filename)
{
	__android_log_print(ANDROID_LOG_WARN, DOLPHIN_TAG, "Getting Description for file: %s", filename.c_str());

	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(filename);

	if (pVolume != nullptr)
	{
		std::map <DiscIO::IVolume::ELanguage, std::string> descriptions = pVolume->GetDescriptions();

		/*
		bool is_wii_title = IsWiiTitle(filename);

		DiscIO::IVolume::ELanguage language = SConfig::GetInstance().m_LocalCoreStartupParameter.GetCurrentLanguage(
				is_wii_title);

		auto it = descriptions.find(language);
		if (it != end)
			return it->second;*/

		auto end = descriptions.end();

		// English tends to be a good fallback when the requested language isn't available
		//if (language != DiscIO::IVolume::ELanguage::LANGUAGE_ENGLISH) {
			auto it = descriptions.find(DiscIO::IVolume::ELanguage::LANGUAGE_ENGLISH);
			if (it != end)
				return it->second;
		//}

		// If English isn't available either, just pick something
		if (!descriptions.empty())
			return descriptions.cbegin()->second;
	}

	return std::string ("");
}

static std::string GetGameId(std::string filename)
{
	__android_log_print(ANDROID_LOG_WARN, DOLPHIN_TAG, "Getting ID for file: %s", filename.c_str());

	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(filename);
	if (pVolume != nullptr)
	{
		std::string id = pVolume->GetUniqueID();
		__android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Game ID: %s", id.c_str());

		return id;
	}
	return std::string ("");
}

static std::string GetApploaderDate(std::string filename)
{
	__android_log_print(ANDROID_LOG_WARN, DOLPHIN_TAG, "Getting Date for file: %s", filename.c_str());

	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(filename);
	if (pVolume != nullptr)
	{
		std::string date = pVolume->GetApploaderDate();
		__android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Date: %s", date.c_str());

		return date;
	}
	return std::string ("");
}

static u64 GetFileSize(std::string filename)
{
	__android_log_print(ANDROID_LOG_WARN, DOLPHIN_TAG, "Getting size of file: %s", filename.c_str());

	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(filename);
	if (pVolume != nullptr)
	{
		u64 size = pVolume->GetSize();
		__android_log_print(ANDROID_LOG_INFO, DOLPHIN_TAG, "Size: %lu", size);

		return  size;
	}

	return -1;
}

static std::string GetJString(JNIEnv *env, jstring jstr)
{
	std::string result = "";
	if (!jstr)
		return result;

	const char *s = env->GetStringUTFChars(jstr, nullptr);
	result = s;
	env->ReleaseStringUTFChars(jstr, s);
	return result;
}

#ifdef __cplusplus
extern "C"
{
#endif

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_UnPauseEmulation(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_PauseEmulation(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_StopEmulation(JNIEnv *env, jobject obj);
JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadEvent(JNIEnv *env, jobject obj, jstring jDevice, jint Button, jint Action);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadMoveEvent(JNIEnv *env, jobject obj, jstring jDevice, jint Axis, jfloat Value);
JNIEXPORT jintArray JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetBanner(JNIEnv *env, jobject obj, jstring jFile);JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetTitle(JNIEnv *env, jobject obj, jstring jFilename);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetDescription(JNIEnv *env, jobject obj, jstring jFilename);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetGameId(JNIEnv *env, jobject obj, jstring jFilename);
JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetCountry(JNIEnv *env, jobject obj, jstring jFilename);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetDate(JNIEnv *env, jobject obj, jstring jFilename);
JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetFilesize(JNIEnv *env, jobject obj, jstring jFilename);
JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_IsWiiTitle(JNIEnv *env, jobject obj, jstring jFilename);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetVersionString(JNIEnv *env, jobject obj);
JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SupportsNEON(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveScreenShot(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_eglBindAPI(JNIEnv *env, jobject obj, jint api);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetConfig(JNIEnv *env, jobject obj, jstring jFile, jstring jSection, jstring jKey, jstring jDefault);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetConfig(JNIEnv *env, jobject obj, jstring jFile, jstring jSection, jstring jKey, jstring jValue);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetFilename(JNIEnv *env, jobject obj, jstring jFile);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveState(JNIEnv *env, jobject obj, jint slot);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadState(JNIEnv *env, jobject obj, jint slot);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_CreateUserFolders(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetUserDirectory(JNIEnv *env, jobject obj, jstring jDirectory);
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetUserDirectory(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Run(JNIEnv *env, jobject obj, jobject _surf);

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_UnPauseEmulation(JNIEnv *env, jobject obj)
{
	PowerPC::Start();
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_PauseEmulation(JNIEnv *env, jobject obj)
{
	PowerPC::Pause();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_StopEmulation(JNIEnv *env, jobject obj)
{
	Core::Stop();
	updateMainFrameEvent.Set(); // Kick the waiting event
}
JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadEvent(JNIEnv *env, jobject obj, jstring jDevice, jint Button, jint Action)
{
	return ButtonManager::GamepadEvent(GetJString(env, jDevice), Button, Action);
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadMoveEvent(JNIEnv *env, jobject obj, jstring jDevice, jint Axis, jfloat Value)
{
	ButtonManager::GamepadAxisEvent(GetJString(env, jDevice), Axis, Value);
}

JNIEXPORT jintArray JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetBanner(JNIEnv *env, jobject obj, jstring jFile)
{
	std::string file = GetJString(env, jFile);
	u32 uBanner[DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT];
	jintArray Banner = env->NewIntArray(DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT);

	if (LoadBanner(file, uBanner))
	{
		env->SetIntArrayRegion(Banner, 0, DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT, (jint*)uBanner);
	}
	return Banner;
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetTitle(JNIEnv *env, jobject obj, jstring jFilename)
{
	std::string filename = GetJString(env, jFilename);
	std::string name = GetTitle(filename);
	return env->NewStringUTF(name.c_str());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetDescription(JNIEnv *env, jobject obj, jstring jFilename)
{
	std::string filename = GetJString(env, jFilename);
	std::string description = GetDescription(filename);
	return env->NewStringUTF(description.c_str());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetGameId(JNIEnv *env, jobject obj, jstring jFilename)
{
	std::string filename = GetJString(env, jFilename);
	std::string id = GetGameId(filename);
	return env->NewStringUTF(id.c_str());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetDate(JNIEnv *env, jobject obj, jstring jFilename)
{
	std::string filename = GetJString(env, jFilename);
	std::string date = GetApploaderDate(filename);
	return env->NewStringUTF(date.c_str());
}

JNIEXPORT jint JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetCountry(JNIEnv *env, jobject obj, jstring jFilename)
{
	std::string filename = GetJString(env, jFilename);
	int country = GetCountry(filename);
	return country;
}

JNIEXPORT jlong JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetFilesize(JNIEnv *env, jobject obj, jstring jFilename)
{
	std::string filename = GetJString(env, jFilename);
	u64 size = GetFileSize(filename);
	return size;
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_IsWiiTitle(JNIEnv *env, jobject obj, jstring jFilename)
{
	std::string filename = GetJString(env, jFilename);
	bool wiiDisc = IsWiiTitle(filename);
	return wiiDisc;
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetVersionString(JNIEnv *env, jobject obj)
{
	return env->NewStringUTF(scm_rev_str);
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SupportsNEON(JNIEnv *env, jobject obj)
{
	return cpu_info.bNEON || cpu_info.bASIMD;
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveScreenShot(JNIEnv *env, jobject obj)
{
	Core::SaveScreenShot();
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_eglBindAPI(JNIEnv *env, jobject obj, jint api)
{
	eglBindAPI(api);
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetConfig(JNIEnv *env, jobject obj, jstring jFile, jstring jSection, jstring jKey, jstring jDefault)
{
	IniFile ini;
	std::string file         = GetJString(env, jFile);
	std::string section      = GetJString(env, jSection);
	std::string key          = GetJString(env, jKey);
	std::string defaultValue = GetJString(env, jDefault);

	ini.Load(File::GetUserPath(D_CONFIG_IDX) + std::string(file));
	std::string value;

	ini.GetOrCreateSection(section)->Get(key, &value, defaultValue);

	return env->NewStringUTF(value.c_str());
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetConfig(JNIEnv *env, jobject obj, jstring jFile, jstring jSection, jstring jKey, jstring jValue)
{
	IniFile ini;
	std::string file         = GetJString(env, jFile);
	std::string section      = GetJString(env, jSection);
	std::string key          = GetJString(env, jKey);
	std::string value        = GetJString(env, jValue);

	ini.Load(File::GetUserPath(D_CONFIG_IDX) + std::string(file));

	ini.GetOrCreateSection(section)->Set(key, value);
	ini.Save(File::GetUserPath(D_CONFIG_IDX) + std::string(file));
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetFilename(JNIEnv *env, jobject obj, jstring jFile)
{
	g_filename = GetJString(env, jFile);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveState(JNIEnv *env, jobject obj, jint slot)
{
	State::Save(slot);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_LoadState(JNIEnv *env, jobject obj, jint slot)
{
	State::Load(slot);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_CreateUserFolders(JNIEnv *env, jobject obj)
{
	File::CreateFullPath(File::GetUserPath(D_CONFIG_IDX));
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX));
	File::CreateFullPath(File::GetUserPath(D_WIIUSER_IDX));
	File::CreateFullPath(File::GetUserPath(D_CACHE_IDX));
	File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
	File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
	File::CreateFullPath(File::GetUserPath(D_HIRESTEXTURES_IDX));
	File::CreateFullPath(File::GetUserPath(D_SCREENSHOTS_IDX));
	File::CreateFullPath(File::GetUserPath(D_STATESAVES_IDX));
	File::CreateFullPath(File::GetUserPath(D_MAILLOGS_IDX));
	File::CreateFullPath(File::GetUserPath(D_SHADERS_IDX));
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + USA_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + EUR_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + JAP_DIR DIR_SEP);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetUserDirectory(JNIEnv *env, jobject obj, jstring jDirectory)
{
	std::string directory = GetJString(env, jDirectory);
	g_set_userpath = directory;
	UICommon::SetUserDirectory(directory);
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetUserDirectory(JNIEnv *env, jobject obj)
{
	return env->NewStringUTF(File::GetUserPath(D_USER_IDX).c_str());
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Run(JNIEnv *env, jobject obj, jobject _surf)
{
	surf = ANativeWindow_fromSurface(env, _surf);

	// Install our callbacks
	OSD::AddCallback(OSD::OSD_INIT, ButtonManager::Init);
	OSD::AddCallback(OSD::OSD_SHUTDOWN, ButtonManager::Shutdown);

	RegisterMsgAlertHandler(&MsgAlert);

	UICommon::SetUserDirectory(g_set_userpath);
	UICommon::Init();

	// No use running the loop when booting fails
	if ( BootManager::BootCore( g_filename.c_str() ) )
		while (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
			updateMainFrameEvent.Wait();

	UICommon::Shutdown();
	ANativeWindow_release(surf);
}


#ifdef __cplusplus
}
#endif
