#include <QApplication>
#include <QMainWindow>
#include "MainWindow.h"

#include "CommonPaths.h"
#include "CPUDetect.h"
#include "ConfigManager.h"
#include "LogManager.h"
#include "HW/Wiimote.h"

#include "VideoBackendBase.h"

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	// TODO: Add command line options:
	// help, debugger, logger, load file, exit on emulation stop, chose video backend, DSP LLE/HLE

	// TODO
/*#if defined _DEBUG && defined _WIN32
    int tmpflag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
    tmpflag |= _CRTDBG_DELAY_FREE_MEM_DF;
    _CrtSetDbgFlag(tmpflag);
#endif*/

    // Register message box and translation handlers
//RegisterMsgAlertHandler( .. );
//RegisterStringTranslator( .. );

    // "ExtendedTrace" looks freakin dangerous!!!
/*#ifdef _WIN32
    EXTENDEDTRACEINITIALIZE(".");
    SetUnhandledExceptionFilter(&MyUnhandledExceptionFilter);
#elif wxUSE_ON_FATAL_EXCEPTION
    wxHandleFatalExceptions(true);
#endif*/


	if (!cpu_info.bSSE2)
    {
		// TODO
/*        Message box with("Hi,\n\nDolphin requires that your CPU has support for SSE2 extensions.\n"
                "Unfortunately your CPU does not support them, so Dolphin will not run.\n\n"
                "Sayonara!\n");*/
        return 0;
    }

#ifdef _WIN32
	// TODO: Check if the file portable exists
#else
    //create all necessary directories in user directory
    //TODO : detect the revision and upgrade where necessary
    File::CopyDir(std::string(SHARED_USER_DIR CONFIG_DIR DIR_SEP).c_str(),
        File::GetUserPath(D_CONFIG_IDX));
    File::CopyDir(std::string(SHARED_USER_DIR GAMECONFIG_DIR DIR_SEP).c_str(),
        File::GetUserPath(D_GAMECONFIG_IDX));
    File::CopyDir(std::string(SHARED_USER_DIR MAPS_DIR DIR_SEP).c_str(),
        File::GetUserPath(D_MAPS_IDX));
    File::CopyDir(std::string(SHARED_USER_DIR SHADERS_DIR DIR_SEP).c_str(),
        File::GetUserPath(D_SHADERS_IDX));
    File::CopyDir(std::string(SHARED_USER_DIR WII_USER_DIR DIR_SEP).c_str(),
        File::GetUserPath(D_WIIUSER_IDX));
    File::CopyDir(std::string(SHARED_USER_DIR OPENCL_DIR DIR_SEP).c_str(),
        File::GetUserPath(D_OPENCL_IDX));

    if (!File::Exists(File::GetUserPath(D_GCUSER_IDX)))
        File::CreateFullPath(File::GetUserPath(D_GCUSER_IDX));
    if (!File::Exists(File::GetUserPath(D_CACHE_IDX)))
        File::CreateFullPath(File::GetUserPath(D_CACHE_IDX));
    if (!File::Exists(File::GetUserPath(D_DUMPDSP_IDX)))
        File::CreateFullPath(File::GetUserPath(D_DUMPDSP_IDX));
    if (!File::Exists(File::GetUserPath(D_DUMPTEXTURES_IDX)))
        File::CreateFullPath(File::GetUserPath(D_DUMPTEXTURES_IDX));
    if (!File::Exists(File::GetUserPath(D_HIRESTEXTURES_IDX)))
        File::CreateFullPath(File::GetUserPath(D_HIRESTEXTURES_IDX));
    if (!File::Exists(File::GetUserPath(D_SCREENSHOTS_IDX)))
        File::CreateFullPath(File::GetUserPath(D_SCREENSHOTS_IDX));
    if (!File::Exists(File::GetUserPath(D_STATESAVES_IDX)))
        File::CreateFullPath(File::GetUserPath(D_STATESAVES_IDX));
    if (!File::Exists(File::GetUserPath(D_MAILLOGS_IDX)))
        File::CreateFullPath(File::GetUserPath(D_MAILLOGS_IDX));
#endif


	// TODO: Move this out of GUI code
	LogManager::Init();
	SConfig::Init();
	VideoBackend::PopulateList();
	VideoBackend::ActivateBackend(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend);
	WiimoteReal::LoadSettings();

	SetEnableAlert(SConfig::GetInstance().m_LocalCoreStartupParameter.bUsePanicHandlers);

	// create main window
	int x = SConfig::GetInstance().m_LocalCoreStartupParameter.iPosX;
    int y = SConfig::GetInstance().m_LocalCoreStartupParameter.iPosY;
    int w = SConfig::GetInstance().m_LocalCoreStartupParameter.iWidth;
    int h = SConfig::GetInstance().m_LocalCoreStartupParameter.iHeight;

    // The following is not needed with X11, where window managers
    // do not allow windows to be created off the desktop.
#ifdef _WIN32
    // Out of desktop check
    HWND hDesktop = GetDesktopWindow();
    RECT rc;
    GetWindowRect(hDesktop, &rc);
    if (rc.right < x + w || rc.bottom < y + h)
        /*x = y = wxDefaultCoord;*/
#elif defined __APPLE__
    if (y < 1)
        /*y = wxDefaultCoord;*/
#endif

	// TODO?
/*#if defined HAVE_X11 && HAVE_X11
    XInitThreads();
#endif */


	DMainWindow window;
	window.move(x, y);
	window.resize(w, h);
	// TODO: Title => svn_rev_str
	// TODO: UseLogger
	window.setMinimumSize(400, 300);
	return app.exec();

	// TODO: On exit:
	// WiimoteReal::Shutdown();
/*#ifdef _WIN32
    if (SConfig::GetInstance().m_WiiAutoUnpair)
        WiimoteReal::UnPair();
#endif*/
	// VideoBackend::ClearList();
	// SConfig::Shutdown();
	// LogManager::Shutdown();

}

void Host_SetStartupDebuggingParameters()
{
    SCoreStartupParameter& StartUp = SConfig::GetInstance().m_LocalCoreStartupParameter;
    StartUp.bEnableDebugging = false;
    StartUp.bBootToPause = false;
}

void* Host_GetInstance()
{
	return NULL;
}

void* Host_GetRenderHandle()
{
	return NULL;
}

bool Host_GetKeyState(int)
{
	return false;
}

void Host_RefreshDSPDebuggerWindow()
{

}

void Host_RequestRenderWindowSize(int, int)
{

}
void Host_GetRenderWindowSize(int&, int&, int&, int&)
{

}

void Host_ConnectWiimote(int, bool)
{

}

void Host_Message(int Id)
{

}

void Host_UpdateMainFrame()
{

}

void Host_UpdateTitle(const char* title)
{
}

void Host_UpdateDisasmDialog()
{
}

void Host_UpdateLogDisplay()
{
}

void Host_UpdateMemoryView()
{
}

void Host_NotifyMapLoaded()
{
}

void Host_UpdateBreakPointView()
{
}

void Host_ShowJitResults(unsigned int address)
{
}

void Host_SetDebugMode(bool enable)
{
}

void Host_RequestWindowSize(int& x, int& y, int& width, int& height)
{
}

bool Host_RendererHasFocus()
{
	return false;
}


void Host_SetWaitCursor(bool enable)
{
}


void Host_UpdateStatusBar(const char* _pText, int Filed = 0)
{
}


void Host_SysMessage(const char *fmt, ...)
{
}

void Host_SetWiiMoteConnectionState(int _State)
{
}

