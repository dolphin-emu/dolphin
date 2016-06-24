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
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0), ^{
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
	});
}

- (void)saveDefaultPreferences
{
	// Must use std::string when passing in strings to config
	// If you don't, the string will be passed as a bool and will always
	// Evaluate to true

	// Dolphin
	IniFile dolphinConfig;
	dolphinConfig.Load(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
	BOOL useJIT = [[NSUserDefaults standardUserDefaults] boolForKey:@"UseJIT"];
	dolphinConfig.GetOrCreateSection("Core")->Set("CPUCore", useJIT ? PowerPC::CORE_JITARM64 : PowerPC::CORE_CACHEDINTERPRETER);
	dolphinConfig.GetOrCreateSection("Core")->Set("CPUThread", YES);
	dolphinConfig.GetOrCreateSection("Core")->Set("Fastmem", NO);
	dolphinConfig.GetOrCreateSection("Core")->Set("GFXBackend", std::string("OGL"));
	dolphinConfig.GetOrCreateSection("Core")->Set("FrameSkip", 2);

	int scale = [UIScreen mainScreen].scale;
	CGSize renderWindowSize = CGSizeMake(renderView.frame.size.width * scale, renderView.frame.size.height * scale);
	dolphinConfig.GetOrCreateSection("Display")->Set("RenderWindowWidth", (int)renderWindowSize.width);
	dolphinConfig.GetOrCreateSection("Display")->Set("RenderWindowHeight", (int)renderWindowSize.height);
	dolphinConfig.Save(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");

	// OpenGL
	IniFile oglConfig;
	oglConfig.Load(File::GetUserPath(D_CONFIG_IDX) + "gfx_opengl.ini");

	IniFile::Section* oglSettings = oglConfig.GetOrCreateSection("Settings");
	oglSettings->Set("ShowFPS", YES);
	oglSettings->Set("ExtendedFPSInfo", YES);
	oglSettings->Set("EFBScale", 2);
	oglSettings->Set("MSAA", 0);
	oglSettings->Set("EnablePixelLighting", YES);
	oglSettings->Set("DisableFog", NO);

	IniFile::Section* oglEnhancements = oglConfig.GetOrCreateSection("Enhancements");
	oglEnhancements->Set("MaxAnisotropy", 0);
	oglEnhancements->Set("ForceFiltering", YES);
	oglEnhancements->Set("StereoSwapEyes", NO);
	oglEnhancements->Set("StereoMode", 0);
	oglEnhancements->Set("StereoDepth", 20);
	oglEnhancements->Set("StereoConvergence", 20);

	IniFile::Section* oglHacks = oglConfig.GetOrCreateSection("Hacks");
	oglHacks->Set("EFBScaledCopy", YES);
	oglHacks->Set("EFBAccessEnable", NO);
	oglHacks->Set("EFBEmulateFormatChanges", NO);
	oglHacks->Set("EFBCopyEnable", NO);
	oglHacks->Set("EFBToTextureEnable", YES);
	oglHacks->Set("EFBCopyCacheEnable", YES);

	oglConfig.Save(File::GetUserPath(D_CONFIG_IDX) + "gfx_opengl.ini");

	// Move Controller Settings
	NSString *configPath = [NSString stringWithCString:File::GetUserPath(D_CONFIG_IDX).c_str()
	                                          encoding:NSUTF8StringEncoding];
	[self copyBundleDirectoryOrFile:@"Config/GCPadNew.ini" toPath:[configPath stringByAppendingPathComponent:@"GCPadNew.ini"]];
	[self copyBundleDirectoryOrFile:@"Config/WiimoteNew.ini" toPath:[configPath stringByAppendingPathComponent:@"WiimoteNew.ini.ini"]];
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
	UICommon::CreateDirectories();
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