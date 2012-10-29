#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QDeclarativeContext>
#include <QIcon>
#include <QGraphicsObject>
#include "CPUDetect.h"

#include "interqt.h"
#include "qmlapplicationviewer/qmlapplicationviewer.h"
#include "main.h"

// TODO: Make this in to a class
QmlApplicationViewer* viewer = NULL;
int main(int argc, char* argv[])
{
// TODO: All low priority comments below
    // Add command line options:
    // help, debugger, logger, load file, exit on emulation stop, chose video backend, DSP LLE/HLE

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

    // TODO for multithreading on X?
/*#if defined HAVE_X11 && HAVE_X11
    XInitThreads();
#endif */


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

    LogManager::Init();
    SConfig::Init();
    VideoBackend::PopulateList();
    VideoBackend::ActivateBackend(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strVideoBackend);
    WiimoteReal::LoadSettings();

    SetEnableAlert(SConfig::GetInstance().m_LocalCoreStartupParameter.bUsePanicHandlers);

    // Start Qt
    QApplication app(argc, argv);

    if (!cpu_info.bSSE2)
    {
        QMessageBox::information(NULL,"Hardware not supported","Hi,\n\nDolphin requires that your CPU has support for SSE2 extensions.\n"
                    "Unfortunately your CPU does not support them, so Dolphin will not run.\n\n"
                    "Sayonara!\n");
        return 0;
    }
    QSettings ui_settings("Dolphin Team", "Dolphin");
    // TODO: Load settings?

    InterQt tools;

    viewer = new QmlApplicationViewer();
    viewer->rootContext()->setContextProperty("currentDir", QDir::currentPath());
    viewer->rootContext()->setContextProperty("tools", &tools);
    viewer->rootContext()->setContextProperty("gameList", &tools.games);
    viewer->setSource(QUrl("qrc:/qml/main.qml"));
    viewer->setWindowIcon(QIcon("qrc:/Dolphin.ico"));
    viewer->setWindowTitle("Dolphin QtQuick" /* + SVN_REV_STR*/);
    viewer->showExpanded(); // Geometry needs to be detected/restored as before.

    // TODO: Use Logger
    int ret = app.exec();
        // Qt has ended

    WiimoteReal::Shutdown();
#ifdef _WIN32
    if (SConfig::GetInstance().m_WiiAutoUnpair)
        WiimoteReal::UnPair();
#endif
    VideoBackend::ClearList();
    SConfig::Shutdown();
    LogManager::Shutdown();
    return ret;
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
    //return (void*)(mainWindow->GetRenderWindow()->winId());
}

bool Host_GetKeyState(int)
{
    return false;
}

void Host_RefreshDSPDebuggerWindow()
{

}

void Host_RequestRenderWindowSize(int w, int h)
{
    QObject *window = viewer->rootObject()->findChild<QObject*>("window");
        if (window == NULL)
        return;
    if (SConfig::GetInstance().m_LocalCoreStartupParameter.bRenderToMain)
    {
        // Make sure to resize the actual client area
        // TODO: Might not work properly, yet.
        //QSize sizediff = mainWindow->size() - mainWindow->GetRenderWindow()->size();
        window->setProperty("width",  w);
        window->setProperty("height", h);

    }
    window->setProperty("width",  w);
    window->setProperty("height", h);
}

// TODO: Rename this to GetRenderClientSize
void Host_GetRenderWindowSize(int& x, int& y, int& width, int& height)
{
    // TODO: Make it more clear what this is supposed to return.. i.e. wxw always sets x=y=0
    x = 0;
    y = 0;
    QObject *window = viewer->rootObject()->findChild<QObject*>("window");
        if (window != NULL)
    {
        width = window->property("width").toInt();
        height = window->property("height").toInt();
    }
}

void Host_ConnectWiimote(int, bool)
{

}

void Host_Message(int Id)
{
    // TODO: Handle IDM_UPDATEGUI, IDM_UPDATESTATUSBAR, IDM_UPDATETITLE, WM_USER_CREATE, IDM_WINDOWSIZEREQUEST, IDM_PANIC(?), IDM_KEYSTATE(?), WM_USER_STOP
}

void Host_UpdateMainFrame()
{

}

void Host_UpdateTitle(const char* title)
{
    viewer->setWindowTitle(QString(title));
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

bool Host_RendererHasFocus()
{
    // TODO: Sometimes with render to main this won't return the correct value
/*  if (mainWindow->GetRenderWindow())
        return mainWindow->GetRenderWindow()->hasFocus();
    else return false;
*/
    return true;
}


void Host_SetWaitCursor(bool enable)
{
}


void Host_UpdateStatusBar(const char* _pText, int Filed = 0)
{
}


void Host_SysMessage(const char *fmt, ...)
{
    // TODO
}

void Host_SetWiiMoteConnectionState(int _State)
{
}

