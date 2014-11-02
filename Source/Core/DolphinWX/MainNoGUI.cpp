// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <getopt.h>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Event.h"
#include "Common/Logging/LogManager.h"

#include "Core/BootManager.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/CoreParameter.h"
#include "Core/Host.h"
#include "Core/State.h"
#include "Core/HW/Wiimote.h"
#include "Core/PowerPC/PowerPC.h"

#include "UICommon/UICommon.h"

#include "VideoCommon/VideoBackendBase.h"

static bool rendererHasFocus = true;
static bool running = true;

class Platform
{
public:
	virtual void Init() = 0;
	virtual void SetTitle(const std::string &title) = 0;
	virtual void MainLoop() = 0;
	virtual void Shutdown() = 0;
	virtual ~Platform() {};
};

static Platform* platform;

void Host_NotifyMapLoaded() {}
void Host_RefreshDSPDebuggerWindow() {}

static Common::Event updateMainFrameEvent;
void Host_Message(int Id)
{
	switch (Id)
	{
		case WM_USER_STOP:
			running = false;
			break;
	}
}

void* windowHandle;
void* Host_GetRenderHandle()
{
	return windowHandle;
}

void Host_UpdateTitle(const std::string& title)
{
	platform->SetTitle(title);
}

void Host_UpdateDisasmDialog(){}

void Host_UpdateMainFrame()
{
	updateMainFrameEvent.Set();
}

void Host_GetRenderWindowSize(int& x, int& y, int& width, int& height)
{
	x = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos;
	y = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos;
	width = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth;
	height = SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight;
}

void Host_RequestRenderWindowSize(int width, int height) {}

void Host_RequestFullscreen(bool enable_fullscreen) {}

void Host_SetStartupDebuggingParameters()
{
	SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;
	StartUp.bEnableDebugging = false;
	StartUp.bBootToPause = false;
}

bool Host_UIHasFocus()
{
	return false;
}

bool Host_RendererHasFocus()
{
	return rendererHasFocus;
}

void Host_ConnectWiimote(int wm_idx, bool connect) {}

void Host_SysMessage(const char *fmt, ...)
{
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	size_t len = strlen(msg);
	if (msg[len - 1] != '\n')
	{
		msg[len - 1] = '\n';
		msg[len] = '\0';
	}

	fprintf(stderr, "%s", msg);
}

void Host_SetWiiMoteConnectionState(int _State) {}

void Host_ShowVideoConfig(void*, const std::string&, const std::string&) {}

#if HAVE_X11
#include <X11/keysym.h>
#include "DolphinWX/X11Utils.h"

class PlatformX11 : public Platform
{
	Display *dpy;
	Window win;
	Cursor blankCursor = None;
#if defined(HAVE_XRANDR) && HAVE_XRANDR
	X11Utils::XRRConfiguration *XRRConfig;
#endif

	void Init() override
	{
		XInitThreads();
		dpy = XOpenDisplay(nullptr);

		win = XCreateSimpleWindow(dpy, DefaultRootWindow(dpy),
					  SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos,
					  SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos,
					  SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth,
					  SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight,
					  0, 0, BlackPixel(dpy, 0));
		XSelectInput(dpy, win, KeyPressMask | FocusChangeMask);
		Atom wmProtocols[1];
		wmProtocols[0] = XInternAtom(dpy, "WM_DELETE_WINDOW", True);
		XSetWMProtocols(dpy, win, wmProtocols, 1);
		XMapRaised(dpy, win);
		XFlush(dpy);
		windowHandle = (void *) win;

		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bDisableScreenSaver)
			X11Utils::InhibitScreensaver(dpy, win, true);

#if defined(HAVE_XRANDR) && HAVE_XRANDR
		XRRConfig = new X11Utils::XRRConfiguration(dpy, win);
#endif

		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
		{
			// make a blank cursor
			Pixmap Blank;
			XColor DummyColor;
			char ZeroData[1] = {0};
			Blank = XCreateBitmapFromData (dpy, win, ZeroData, 1, 1);
			blankCursor = XCreatePixmapCursor(dpy, Blank, Blank, &DummyColor, &DummyColor, 0, 0);
			XFreePixmap (dpy, Blank);
			XDefineCursor(dpy, win, blankCursor);
		}
	}

	void SetTitle(const std::string &string) override
	{
		XStoreName(dpy, win, string.c_str());
	}

	void MainLoop() override
	{
		bool fullscreen = SConfig::GetInstance().m_LocalCoreStartupParameter.bFullscreen;

		if (fullscreen)
		{
			X11Utils::ToggleFullscreen(dpy, win);
#if defined(HAVE_XRANDR) && HAVE_XRANDR
			XRRConfig->ToggleDisplayMode(True);
#endif
		}

		// The actual loop
		while (running)
		{
			XEvent event;
			KeySym key;
			for (int num_events = XPending(dpy); num_events > 0; num_events--)
			{
				XNextEvent(dpy, &event);
				switch (event.type)
				{
				case KeyPress:
					key = XLookupKeysym((XKeyEvent*)&event, 0);
					if (key == XK_Escape)
					{
						if (Core::GetState() == Core::CORE_RUN)
						{
							if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
								XUndefineCursor(dpy, win);
							Core::SetState(Core::CORE_PAUSE);
						}
						else
						{
							if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
								XDefineCursor(dpy, win, blankCursor);
							Core::SetState(Core::CORE_RUN);
						}
					}
					else if ((key == XK_Return) && (event.xkey.state & Mod1Mask))
					{
						fullscreen = !fullscreen;
						X11Utils::ToggleFullscreen(dpy, win);
#if defined(HAVE_XRANDR) && HAVE_XRANDR
						XRRConfig->ToggleDisplayMode(fullscreen);
#endif
					}
					else if (key >= XK_F1 && key <= XK_F8)
					{
						int slot_number = key - XK_F1 + 1;
						if (event.xkey.state & ShiftMask)
							State::Save(slot_number);
						else
							State::Load(slot_number);
					}
					else if (key == XK_F9)
						Core::SaveScreenShot();
					else if (key == XK_F11)
						State::LoadLastSaved();
					else if (key == XK_F12)
					{
						if (event.xkey.state & ShiftMask)
							State::UndoLoadState();
						else
							State::UndoSaveState();
					}
					break;
				case FocusIn:
					rendererHasFocus = true;
					if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor &&
					    Core::GetState() != Core::CORE_PAUSE)
						XDefineCursor(dpy, win, blankCursor);
					break;
				case FocusOut:
					rendererHasFocus = false;
					if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
						XUndefineCursor(dpy, win);
					break;
				case ClientMessage:
					if ((unsigned long) event.xclient.data.l[0] == XInternAtom(dpy, "WM_DELETE_WINDOW", False))
						running = false;
					break;
				}
			}
			if (!fullscreen)
			{
				Window winDummy;
				unsigned int borderDummy, depthDummy;
				XGetGeometry(dpy, win, &winDummy,
					     &SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowXPos,
					     &SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowYPos,
					     (unsigned int *)&SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowWidth,
					     (unsigned int *)&SConfig::GetInstance().m_LocalCoreStartupParameter.iRenderWindowHeight,
					     &borderDummy, &depthDummy);
			}
			usleep(100000);
		}
	}

	void Shutdown() override
	{
#if defined(HAVE_XRANDR) && HAVE_XRANDR
		delete XRRConfig;
#endif

		if (SConfig::GetInstance().m_LocalCoreStartupParameter.bHideCursor)
			XFreeCursor(dpy, blankCursor);

		XCloseDisplay(dpy);
	}
};
#endif

static Platform* GetPlatform()
{
#if HAVE_X11
	return new PlatformX11();
#endif
	return nullptr;
}

int main(int argc, char* argv[])
{
	int ch, help = 0;
	struct option longopts[] = {
		{ "exec",    no_argument, nullptr, 'e' },
		{ "help",    no_argument, nullptr, 'h' },
		{ "version", no_argument, nullptr, 'v' },
		{ nullptr,      0,           nullptr,  0  }
	};

	while ((ch = getopt_long(argc, argv, "eh?v", longopts, 0)) != -1)
	{
		switch (ch)
		{
		case 'e':
			break;
		case 'h':
		case '?':
			help = 1;
			break;
		case 'v':
			fprintf(stderr, "%s\n", scm_rev_str);
			return 1;
		}
	}

	if (help == 1 || argc == optind)
	{
		fprintf(stderr, "%s\n\n", scm_rev_str);
		fprintf(stderr, "A multi-platform GameCube/Wii emulator\n\n");
		fprintf(stderr, "Usage: %s [-e <file>] [-h] [-v]\n", argv[0]);
		fprintf(stderr, "  -e, --exec   Load the specified file\n");
		fprintf(stderr, "  -h, --help   Show this help message\n");
		fprintf(stderr, "  -v, --help   Print version and exit\n");
		return 1;
	}

	platform = GetPlatform();
	if (!platform)
	{
		fprintf(stderr, "No platform found\n");
		return 1;
	}

	UICommon::Init();

	platform->Init();

	if (!BootManager::BootCore(argv[optind]))
	{
		fprintf(stderr, "Could not boot %s\n", argv[optind]);
		return 1;
	}

	while (!Core::IsRunning())
		updateMainFrameEvent.Wait();

	platform->MainLoop();
	Core::Stop();
	while (PowerPC::GetState() != PowerPC::CPU_POWERDOWN)
		updateMainFrameEvent.Wait();

	Core::Shutdown();
	platform->Shutdown();
	UICommon::Shutdown();

	delete platform;

	return 0;
}
