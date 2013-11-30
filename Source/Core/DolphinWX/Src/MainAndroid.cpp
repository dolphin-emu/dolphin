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

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "Common.h"
#include "FileUtil.h"

#include "Core.h"
#include "Host.h"
#include "CPUDetect.h"
#include "Thread.h"

#include "State.h"
#include "PowerPC/PowerPC.h"
#include "HW/Wiimote.h"

#include "VideoBackendBase.h"
#include "ConfigManager.h"
#include "LogManager.h"
#include "BootManager.h"
#include "OnScreenDisplay.h"

// Banner loading
#include "Filesystem.h"
#include "BannerLoader.h"
#include "VolumeCreator.h"

#include "Android/ButtonManager.h"

#include <jni.h>
#include <android/log.h>
#include <android/native_window_jni.h>
ANativeWindow* surf;
int g_width, g_height;
std::string g_filename;
static std::thread g_run_thread;

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "Dolphinemu", __VA_ARGS__))

void Host_NotifyMapLoaded() {}
void Host_RefreshDSPDebuggerWindow() {}

void Host_ShowJitResults(unsigned int address){}

Common::Event updateMainFrameEvent;
void Host_Message(int Id)
{
}

void* Host_GetRenderHandle()
{
	return surf;
}

void* Host_GetInstance() { return NULL; }

void Host_UpdateTitle(const char* title)
{
	LOGI(title);
};

void Host_UpdateLogDisplay(){}

void Host_UpdateDisasmDialog(){}

void Host_UpdateMainFrame()
{
}

void Host_UpdateBreakPointView(){}

bool Host_GetKeyState(int keycode)
{
	return false;
}

void Host_GetRenderWindowSize(int& x, int& y, int& width, int& height)
{
	x = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos;
	y = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos;
	width = g_width;
	height = g_height;
}

void Host_RequestRenderWindowSize(int width, int height) {}
void Host_SetStartupDebuggingParameters()
{
}

bool Host_RendererHasFocus()
{
	return true;
}

void Host_ConnectWiimote(int wm_idx, bool connect) {}

void Host_SetWaitCursor(bool enable){}

void Host_UpdateStatusBar(const char* _pText, int Filed){}

void Host_SysMessage(const char *fmt, ...)
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsnprintf(msg, 512, fmt, list);
	va_end(list);

	size_t len = strlen(msg);
	if (msg[len - 1] != '\n') {
		msg[len - 1] = '\n';
		msg[len] = '\0';
	}
	LOGI(msg);
}

void Host_SetWiiMoteConnectionState(int _State) {}

#define DVD_BANNER_WIDTH 96
#define DVD_BANNER_HEIGHT 32
std::vector<std::string> m_volume_names;
std::vector<std::string> m_names;

static inline u32 Average32(u32 a, u32 b) {
	return ((a >> 1) & 0x7f7f7f7f) + ((b >> 1) & 0x7f7f7f7f);
}

static inline u32 GetPixel(u32 *buffer, unsigned int x, unsigned int y) {
	// thanks to unsignedness, these also check for <0 automatically.
	if (x > 191) return 0;
	if (y > 63) return 0;
	return buffer[y * 192 + x];
}

bool LoadBanner(std::string filename, u32 *Banner)
{
	DiscIO::IVolume* pVolume = DiscIO::CreateVolumeFromFilename(filename);

	if (pVolume != NULL)
	{
		bool bIsWad = false;
		if (DiscIO::IsVolumeWadFile(pVolume))
			bIsWad = true;

		m_volume_names = pVolume->GetNames();

		// check if we can get some info from the banner file too
		DiscIO::IFileSystem* pFileSystem = DiscIO::CreateFileSystem(pVolume);

		if (pFileSystem != NULL || bIsWad)
		{
			DiscIO::IBannerLoader* pBannerLoader = DiscIO::CreateBannerLoader(*pFileSystem, pVolume);

			if (pBannerLoader != NULL)
				if (pBannerLoader->IsValid())
				{
					m_names = pBannerLoader->GetNames();
					int Width, Height;
					std::vector<u32> BannerVec = pBannerLoader->GetBanner(&Width, &Height);
					// This code (along with above inlines) is moved from
					// elsewhere.  Someone who knows anything about Android
					// please get rid of it and use proper high-resolution
					// images.
					if (Height == 64)
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
					}
					else
					{
						memcpy(Banner, &BannerVec[0], 96 * 32 * 4);
					}
					return true;
				}
		}
	}

	return false;
}
std::string GetName(std::string filename)
{
	if (!m_names.empty())
		return m_names[0];

	if (!m_volume_names.empty())
		return m_volume_names[0];
	// No usable name, return filename (better than nothing)
	std::string name;
	SplitPath(filename, NULL, &name, NULL);

	return name;
}


#ifdef __cplusplus
extern "C"
{
#endif
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
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onTouchEvent(JNIEnv *env, jobject obj, jint padID, jint Button, jint Action)
{
	ButtonManager::TouchEvent(padID, (ButtonManager::ButtonType)Button, Action);
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onTouchAxisEvent(JNIEnv *env, jobject obj, jint padID, jint Button, jfloat Action)
{
	ButtonManager::TouchAxisEvent(padID, (ButtonManager::ButtonType)Button, Action);
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadEvent(JNIEnv *env, jobject obj, jstring jDevice, jint Button, jint Action)
{
	const char *Device = env->GetStringUTFChars(jDevice, NULL);
	std::string strDevice = std::string(Device);
	ButtonManager::GamepadEvent(strDevice, (ButtonManager::ButtonType)Button, Action);
	env->ReleaseStringUTFChars(jDevice, Device);
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_onGamePadMoveEvent(JNIEnv *env, jobject obj, jstring jDevice, jint Axis, jfloat Value)
{
	const char *Device = env->GetStringUTFChars(jDevice, NULL);
	std::string strDevice = std::string(Device);
	ButtonManager::GamepadAxisEvent(strDevice, (ButtonManager::ButtonType)Axis, Value);
	env->ReleaseStringUTFChars(jDevice, Device);
}

JNIEXPORT jintArray JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetBanner(JNIEnv *env, jobject obj, jstring jFile)
{
	const char *File = env->GetStringUTFChars(jFile, NULL);
	jintArray Banner = env->NewIntArray(DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT);
	u32 uBanner[DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT];
	if (LoadBanner(File, uBanner))
	{
		env->SetIntArrayRegion(Banner, 0, DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT, (jint*)uBanner);
	}
	env->ReleaseStringUTFChars(jFile, File);
	return Banner;
}
JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetTitle(JNIEnv *env, jobject obj, jstring jFile)
{
	const char *File = env->GetStringUTFChars(jFile, NULL);
	std::string Name = GetName(File);
	m_names.clear();
	m_volume_names.clear();

	env->ReleaseStringUTFChars(jFile, File);
	return env->NewStringUTF(Name.c_str());
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetVersionString(JNIEnv *env, jobject obj)
{
	return env->NewStringUTF(scm_rev_str);
}

JNIEXPORT jboolean JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SupportsNEON(JNIEnv *env, jobject obj)
{
	return cpu_info.bNEON;
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SaveScreenShot(JNIEnv *env, jobject obj)
{
	Core::SaveScreenShot();
}

JNIEXPORT jstring JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_GetConfig(JNIEnv *env, jobject obj, jstring jFile, jstring jKey, jstring jValue, jstring jDefault)
{
	IniFile ini;
	const char *File = env->GetStringUTFChars(jFile, NULL);
	const char *Key = env->GetStringUTFChars(jKey, NULL);
	const char *Value = env->GetStringUTFChars(jValue, NULL);
	const char *Default = env->GetStringUTFChars(jDefault, NULL);

	ini.Load(File::GetUserPath(D_CONFIG_IDX) + std::string(File));
	std::string value;

	ini.Get(Key, Value, &value, Default);

	env->ReleaseStringUTFChars(jFile, File);
	env->ReleaseStringUTFChars(jKey, Key);
	env->ReleaseStringUTFChars(jValue, Value);
	env->ReleaseStringUTFChars(jDefault, Default);

	return env->NewStringUTF(value.c_str());
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetConfig(JNIEnv *env, jobject obj, jstring jFile, jstring jKey, jstring jValue, jstring jDefault)
{
	IniFile ini;
	const char *File = env->GetStringUTFChars(jFile, NULL);
	const char *Key = env->GetStringUTFChars(jKey, NULL);
	const char *Value = env->GetStringUTFChars(jValue, NULL);
	const char *Default = env->GetStringUTFChars(jDefault, NULL);

	ini.Load(File::GetUserPath(D_CONFIG_IDX) + std::string(File));

	ini.Set(Key, Value, Default);
	ini.Save(File::GetUserPath(D_CONFIG_IDX) + std::string(File));

	env->ReleaseStringUTFChars(jFile, File);
	env->ReleaseStringUTFChars(jKey, Key);
	env->ReleaseStringUTFChars(jValue, Value);
	env->ReleaseStringUTFChars(jDefault, Default);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetFilename(JNIEnv *env, jobject obj, jstring jFile)
{
	const char *File = env->GetStringUTFChars(jFile, NULL);

	g_filename = std::string(File);

	env->ReleaseStringUTFChars(jFile, File);
}
JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_SetDimensions(JNIEnv *env, jobject obj, jint _width, jint _height)
{
	g_width = (int)_width;
	g_height = (int)_height;
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
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + USA_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + EUR_DIR DIR_SEP);
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX) + JAP_DIR DIR_SEP);
}

JNIEXPORT void JNICALL Java_org_dolphinemu_dolphinemu_NativeLibrary_Run(JNIEnv *env, jobject obj, jobject _surf)
{
	surf = ANativeWindow_fromSurface(env, _surf);
	// Install our callbacks
	OSD::AddCallback(OSD::OSD_INIT, ButtonManager::Init);
	OSD::AddCallback(OSD::OSD_SHUTDOWN, ButtonManager::Shutdown);

	LogManager::Init();
	SConfig::Init();
	VideoBackend::PopulateList();
	VideoBackend::ActivateBackend(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend);
	WiimoteReal::LoadSettings();

	// No use running the loop when booting fails
	if ( BootManager::BootCore( g_filename.c_str() ) )
		while (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
			updateMainFrameEvent.Wait();

	WiimoteReal::Shutdown();
	VideoBackend::ClearList();
	SConfig::Shutdown();
	LogManager::Shutdown();
	ANativeWindow_release(surf);
}


#ifdef __cplusplus
}
#endif
