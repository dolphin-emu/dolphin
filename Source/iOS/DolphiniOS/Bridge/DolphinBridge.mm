// Copyright 2016 OatmealDome, WillCobb
// Licensed under GPLV2+
// Refer to the license.txt provided

#import "Bridge/DolphinBridge.h"

#import <Foundation/Foundation.h>
#import "AppDelegate.h"

#include <cstdio>
#include <cstdlib>

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/CPUDetect.h"
#include "Common/Event.h"
#include "Common/FileUtil.h"
#include "Common/Logging/ConsoleListener.h"
#include "Common/Logging/LogManager.h"

#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/Host.h"
#include "Core/State.h"
#include "Core/HW/Wiimote.h"
#include "Core/PowerPC/PowerPC.h"

#include "DiscIO/Filesystem.h"
#include "DiscIO/VolumeCreator.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VideoBackendBase.h"

#include "Common/Logging/ConsoleListener.h"

GLKView* renderView;

@implementation DolphinBridge : NSObject

- (id)init
{
	if (self = [super init]) {
		[self createUserFoldersAtPath:[AppDelegate libraryPath]];
		[self copyResourcesToPath:[AppDelegate documentsPath]];
		[self saveDefaultPreferences]; // Save every run until a settings ui is implemented.
	}
	return self;
}

- (void)openRomAtPath:(NSString* )path inView:(GLKView *)view;
{
	renderView = view;
	NSLog(@"Loading game at path: %@", path);
	UICommon::Init();

	if (BootManager::BootCore([path UTF8String]))
	{
		NSLog(@"Booted Core");
	}
	else
	{
		NSLog(@"Unable to boot");
		return;
	}
	while (!Core::IsRunning())
	{
		NSLog(@"Waiting for run");
		usleep(100000);
	}
	while (Core::IsRunning())
	{
		updateMainFrameEvent.Wait();
		Core::HostDispatchJobs();
	}
	UICommon::Shutdown();
}

- (void)saveDefaultPreferences
{
	
	// Dolphin
	IniFile dolphinConfig;
	dolphinConfig.Load(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
	BOOL useJIT = [[NSUserDefaults standardUserDefaults] boolForKey:@"UseJIT"];
	dolphinConfig.GetOrCreateSection("Core")->Set("CPUCore", useJIT ? PowerPC::CORE_JITARM64 : PowerPC::CORE_CACHEDINTERPRETER);
	dolphinConfig.GetOrCreateSection("Core")->Set("CPUThread", "True");
	dolphinConfig.GetOrCreateSection("Core")->Set("Fastmem", "False");
	dolphinConfig.GetOrCreateSection("Core")->Set("GFXBackend", "OGL");
	dolphinConfig.GetOrCreateSection("Core")->Set("FrameSkip", "0x00000000");
	dolphinConfig.Save(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");

	// OpenGL
	IniFile oglConfig;
	oglConfig.Load(File::GetUserPath(D_CONFIG_IDX) + "gfx_opengl.ini");

	IniFile::Section* oglSettings = oglConfig.GetOrCreateSection("Settings");
	oglSettings->Set("ShowFPS", "True");
	oglSettings->Set("EFBScale", "2");
	oglSettings->Set("MSAA", "0");
	oglSettings->Set("EnablePixelLighting", "False");
	oglSettings->Set("DisableFog", "False");

	IniFile::Section* oglEnhancements = oglConfig.GetOrCreateSection("Enhancements");
	oglEnhancements->Set("MaxAnisotropy", "0");
	oglEnhancements->Set("ForceFiltering", "False");
	oglEnhancements->Set("StereoSwapEyes", "False");
	oglEnhancements->Set("StereoMode", "0");
	oglEnhancements->Set("StereoDepth", "20");
	oglEnhancements->Set("StereoConvergence", "20");

	IniFile::Section* oglHacks = oglConfig.GetOrCreateSection("Hacks");
	oglHacks->Set("EFBScaledCopy", "True");
	oglHacks->Set("EFBAccessEnable", "False");
	oglHacks->Set("EFBEmulateFormatChanges", "False");
	oglHacks->Set("EFBCopyEnable", "True");
	oglHacks->Set("EFBToTextureEnable", "True");
	oglHacks->Set("EFBCopyCacheEnable", "False");

	oglConfig.Save(File::GetUserPath(D_CONFIG_IDX) + "gfx_opengl.ini");

	// Move Controller Settings
	NSString *configPath = [NSString stringWithCString:File::GetUserPath(D_GCUSER_IDX).c_str()
	                                          encoding:NSUTF8StringEncoding];
	[self copyBundleDirectoryOrFile:@"Config/GCPadNew.ini" toPath:configPath];
	[self copyBundleDirectoryOrFile:@"Config/WiimoteNew.ini" toPath:configPath];
}

-(void)copyResourcesToPath:(NSString*)resourcesPath
{
	NSFileManager* fileManager = [NSFileManager defaultManager];
	if (![fileManager fileExistsAtPath: [resourcesPath stringByAppendingString:@"GC"]])
	{
		NSLog(@"Copying GC folder...");
		[self copyBundleDirectoryOrFile:@"GC" toPath:resourcesPath];
	}
	if (![fileManager fileExistsAtPath: [resourcesPath stringByAppendingString:@"Shaders"]])
	{
		NSLog(@"Copying Shaders folder...");
		[self copyBundleDirectoryOrFile:@"Shaders" toPath:resourcesPath];
	}
}

- (void)copyBundleDirectoryOrFile:(NSString* )file toPath:(NSString*)destination
{
	NSString* source = [[[[NSBundle mainBundle] resourcePath] stringByAppendingString: @"/"] stringByAppendingString:file];
	NSLog(@"copyDirectory source: %@", source);
	NSError* err = nil;
	if (![[NSFileManager defaultManager] copyItemAtPath:source toPath:destination error:&err])
	{
		NSLog(@"Error copying directory: %@", [err localizedDescription]);
	}
}

- (void)createUserFoldersAtPath:(NSString*)path
{
	std::string directory([path cStringUsingEncoding:NSUTF8StringEncoding]);
	UICommon::SetUserDirectory(directory);
	File::CreateFullPath(File::GetUserPath(D_CONFIG_IDX));
	File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX));
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

#pragma mark - Host Calls

Common::Event updateMainFrameEvent;
void Host_Message(int Id)
{
	if (Id == WM_USER_JOB_DISPATCH) {
		updateMainFrameEvent.Set();
	}
}

ConsoleListener::ConsoleListener()
{
}

ConsoleListener::~ConsoleListener()
{
}

void ConsoleListener::Log(LogTypes::LOG_LEVELS level, const char* text)
{
	switch (level)
	{
		case LogTypes::LOG_LEVELS::LDEBUG:
			printf("Debug: ");
			break;
		case LogTypes::LOG_LEVELS::LINFO:
			printf("Info: ");
			break;
		case LogTypes::LOG_LEVELS::LWARNING:
			printf("Warning: ");
			break;
		case LogTypes::LOG_LEVELS::LERROR:
			printf("Error: ");
			break;
		case LogTypes::LOG_LEVELS::LNOTICE:
			printf("Notice: ");
			break;
	}
	printf("%s\n", text);
}

void* Host_GetRenderHandle()
{
	return (__bridge void*)renderView;
}

void Host_UpdateTitle(const std::string& title)
{
}

void Host_UpdateDisasmDialog()
{
}

void Host_UpdateMainFrame()
{
}

void Host_NotifyMapLoaded()
{
}

void Host_RefreshDSPDebuggerWindow()
{
}

void Host_RequestRenderWindowSize(int width, int height)
{
}

void Host_RequestFullscreen(bool enable_fullscreen)
{
}

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

void Host_ConnectWiimote(int wm_idx, bool connect)
{
}

void Host_SetWiiMoteConnectionState(int _State)
{
}

void Host_ShowVideoConfig(void*, const std::string&, const std::string&)
{
}



@end