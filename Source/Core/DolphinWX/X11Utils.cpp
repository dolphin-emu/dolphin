// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>

#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreParameter.h"
#include "DolphinWX/X11Utils.h"

extern char **environ;

#if defined(HAVE_WX) && HAVE_WX
#include <algorithm>
#include <string>

#include "DolphinWX/WxUtils.h"
#endif

namespace X11Utils
{

void ToggleFullscreen(Display *dpy, Window win)
{
	// Init X event structure for _NET_WM_STATE_FULLSCREEN client message
	XEvent event;
	event.xclient.type = ClientMessage;
	event.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", False);
	event.xclient.window = win;
	event.xclient.format = 32;
	event.xclient.data.l[0] = _NET_WM_STATE_TOGGLE;
	event.xclient.data.l[1] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);

	// Send the event
	if (!XSendEvent(dpy, DefaultRootWindow(dpy), False,
	                SubstructureRedirectMask | SubstructureNotifyMask, &event))
		ERROR_LOG(VIDEO, "Failed to switch fullscreen/windowed mode.");
}

void InhibitScreensaver(Display *dpy, Window win, bool suspend)
{
	char id[11];
	snprintf(id, sizeof(id), "0x%lx", win);

	// Call xdg-screensaver
	char *argv[4] = {
		(char *)"xdg-screensaver",
		(char *)(suspend ? "suspend" : "resume"),
		id,
		nullptr};
	pid_t pid;
	if (!posix_spawnp(&pid, "xdg-screensaver", nullptr, nullptr, argv, environ))
	{
		int status;
		while (waitpid (pid, &status, 0) == -1);

		DEBUG_LOG(VIDEO, "Started xdg-screensaver (PID = %d)", (int)pid);
	}
}

#if defined(HAVE_XRANDR) && HAVE_XRANDR
XRRConfiguration::XRRConfiguration(Display *_dpy, Window _win)
	: dpy(_dpy)
	, win(_win)
	, screenResources(nullptr), outputInfo(nullptr), crtcInfo(nullptr)
	, fullMode(0)
	, fs_fb_width(0), fs_fb_height(0), fs_fb_width_mm(0), fs_fb_height_mm(0)
	, bValid(true), bIsFullscreen(false)
{
	int XRRMajorVersion, XRRMinorVersion;

	if (!XRRQueryVersion(dpy, &XRRMajorVersion, &XRRMinorVersion) ||
	    (XRRMajorVersion < 1 || (XRRMajorVersion == 1 && XRRMinorVersion < 3)))
	{
		WARN_LOG(VIDEO, "XRRExtension not supported.");
		bValid = false;
		return;
	}

	screenResources = XRRGetScreenResourcesCurrent(dpy, win);

	screen = DefaultScreen(dpy);
	fb_width = DisplayWidth(dpy, screen);
	fb_height = DisplayHeight(dpy, screen);
	fb_width_mm = DisplayWidthMM(dpy, screen);
	fb_height_mm = DisplayHeightMM(dpy, screen);

	INFO_LOG(VIDEO, "XRRExtension-Version %d.%d", XRRMajorVersion, XRRMinorVersion);
	Update();
}

XRRConfiguration::~XRRConfiguration()
{
	if (bValid && bIsFullscreen)
		ToggleDisplayMode(False);
	if (screenResources)
		XRRFreeScreenResources(screenResources);
	if (outputInfo)
		XRRFreeOutputInfo(outputInfo);
	if (crtcInfo)
		XRRFreeCrtcInfo(crtcInfo);
}

void XRRConfiguration::Update()
{
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution == "Auto")
		return;

	if (!bValid)
		return;

	if (outputInfo)
	{
		XRRFreeOutputInfo(outputInfo);
		outputInfo = nullptr;
	}
	if (crtcInfo)
	{
		XRRFreeCrtcInfo(crtcInfo);
		crtcInfo = nullptr;
	}
	fullMode = 0;

	// Get the resolution setings for fullscreen mode
	unsigned int fullWidth, fullHeight;
	char *output_name = nullptr;
	if (SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution.find(':') ==
			std::string::npos)
	{
		fullWidth = fb_width;
		fullHeight = fb_height;
	}
	else
	{
		sscanf(SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution.c_str(),
				"%m[^:]: %ux%u", &output_name, &fullWidth, &fullHeight);
	}

	for (int i = 0; i < screenResources->noutput; i++)
	{
		XRROutputInfo *output_info = XRRGetOutputInfo(dpy, screenResources, screenResources->outputs[i]);
		if (output_info && output_info->crtc && output_info->connection == RR_Connected)
		{
			XRRCrtcInfo *crtc_info = XRRGetCrtcInfo(dpy, screenResources, output_info->crtc);
			if (crtc_info)
			{
				if (!output_name || !strcmp(output_name, output_info->name))
				{
					// Use the first output for the default setting.
					if (!output_name)
					{
						output_name = strdup(output_info->name);
						SConfig::GetInstance().m_LocalCoreStartupParameter.strFullscreenResolution =
							StringFromFormat("%s: %ux%u", output_info->name, fullWidth, fullHeight);
					}
					outputInfo = output_info;
					crtcInfo = crtc_info;
					for (int j = 0; j < output_info->nmode && fullMode == 0; j++)
					{
						for (int k = 0; k < screenResources->nmode && fullMode == 0; k++)
						{
							if (output_info->modes[j] == screenResources->modes[k].id)
							{
								if (fullWidth == screenResources->modes[k].width &&
										fullHeight == screenResources->modes[k].height)
								{
									fullMode = screenResources->modes[k].id;
									if (crtcInfo->x + (int)screenResources->modes[k].width > fs_fb_width)
										fs_fb_width = crtcInfo->x + screenResources->modes[k].width;
									if (crtcInfo->y + (int)screenResources->modes[k].height > fs_fb_height)
										fs_fb_height = crtcInfo->y + screenResources->modes[k].height;
								}
							}
						}
					}
				}
				else
				{
					if (crtc_info->x + (int)crtc_info->width > fs_fb_width)
						fs_fb_width = crtc_info->x + crtc_info->width;
					if (crtc_info->y + (int)crtc_info->height > fs_fb_height)
						fs_fb_height = crtc_info->y + crtc_info->height;
				}
			}
			if (crtc_info && crtcInfo != crtc_info)
				XRRFreeCrtcInfo(crtc_info);
		}
		if (output_info && outputInfo != output_info)
			XRRFreeOutputInfo(output_info);
	}
	fs_fb_width_mm = fs_fb_width * DisplayHeightMM(dpy, screen) / DisplayHeight(dpy, screen);
	fs_fb_height_mm = fs_fb_height * DisplayHeightMM(dpy, screen) / DisplayHeight(dpy, screen);

	if (output_name)
		free(output_name);

	if (outputInfo && crtcInfo && fullMode)
	{
		INFO_LOG(VIDEO, "Fullscreen Resolution %dx%d", fullWidth, fullHeight);
	}
	else
	{
		ERROR_LOG(VIDEO, "Failed to obtain fullscreen size.\n"
				"Using current desktop resolution for fullscreen.");
	}
}

void XRRConfiguration::ToggleDisplayMode(bool bFullscreen)
{
	if (!bValid || !screenResources || !outputInfo || !crtcInfo || !fullMode)
		return;
	if (bFullscreen == bIsFullscreen)
		return;

	XGrabServer(dpy);
	if (bFullscreen)
	{
		XRRSetCrtcConfig(dpy, screenResources, outputInfo->crtc, CurrentTime,
				crtcInfo->x, crtcInfo->y, fullMode, crtcInfo->rotation,
				crtcInfo->outputs, crtcInfo->noutput);
		XRRSetScreenSize(dpy, win, fs_fb_width, fs_fb_height, fs_fb_width_mm, fs_fb_height_mm);
		bIsFullscreen = true;
	}
	else
	{
		XRRSetCrtcConfig(dpy, screenResources, outputInfo->crtc, CurrentTime,
				crtcInfo->x, crtcInfo->y, crtcInfo->mode, crtcInfo->rotation,
				crtcInfo->outputs, crtcInfo->noutput);
		XRRSetScreenSize(dpy, win, fb_width, fb_height, fb_width_mm, fb_height_mm);
		bIsFullscreen = false;
	}
	XUngrabServer(dpy);
	XSync(dpy, false);
}

void XRRConfiguration::AddResolutions(std::vector<std::string>& resos)
{
	if (!bValid || !screenResources)
		return;

	//Get all full screen resolutions for the config dialog
	for (int i = 0; i < screenResources->noutput; i++)
	{
		XRROutputInfo *output_info =
			XRRGetOutputInfo(dpy, screenResources, screenResources->outputs[i]);

		if (output_info && output_info->crtc && output_info->connection == RR_Connected)
		{
			for (int j = 0; j < output_info->nmode; j++)
			{
				for (int k = 0; k < screenResources->nmode; k++)
				{
					if (output_info->modes[j] == screenResources->modes[k].id)
					{
						const std::string strRes =
							std::string(output_info->name) + ": " +
							std::string(screenResources->modes[k].name);
						// Only add unique resolutions
						if (std::find(resos.begin(), resos.end(), strRes) == resos.end())
						{
							resos.push_back(strRes);
						}
					}
				}
			}
		}
		if (output_info)
			XRRFreeOutputInfo(output_info);
	}
}

#endif

}
