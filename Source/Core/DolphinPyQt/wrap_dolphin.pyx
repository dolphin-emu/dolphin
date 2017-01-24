from libcpp cimport bool
from libcpp.string cimport string

from Qt cimport QPixmap as QPixmap_t

cimport BootManager as dol_BootManager
cimport Common as dol_Common
cimport Core as dol_Core
cimport Movie as dol_Movie
cimport Resources as dol_Resources
cimport ProcessorInterface as dol_ProcessorInterface
cimport State as dol_State
cimport UICommon as dol_UICommon

import sys

from PyQt5.QtGui import QPixmap
import sip

# Wrap Dolphin namespaces as classes, so that
# the Python code looks similar to C++ code

class BootManager:
    def BootCore(fn):
        return dol_BootManager.BootCore(fn.encode(sys.getfilesystemencoding()))
    def Stop():
        dol_BootManager.Stop()

class Common:
    WM_USER_STOP = dol_Common.WM_USER_STOP
    WM_USER_CREATE = dol_Common.WM_USER_CREATE
    WM_USER_SETCURSOR = dol_Common.WM_USER_SETCURSOR
    WM_USER_JOB_DISPATCH = dol_Common.WM_USER_JOB_DISPATCH

class Core:
    def HostDispatchJobs():
        dol_Core.HostDispatchJobs()
    def Shutdown():
        dol_Core.Shutdown()
    def SaveScreenShot():
        dol_Core.SaveScreenShot()
    def GetState():
        return dol_Core.GetState()
    def SetState(state):
        dol_Core.SetState(state)

    CORE_UNINITIALIZED = dol_Core.CORE_UNINITIALIZED
    CORE_PAUSE = dol_Core.CORE_PAUSE
    CORE_RUN = dol_Core.CORE_RUN
    CORE_STOPPING = dol_Core.CORE_STOPPING

class Movie:
    def IsRecordingInput():
        return dol_Movie.IsRecordingInput()
    def SetReset(reset):
        dol_Movie.SetReset(reset)
    def DoFrameStep():
        dol_Movie.DoFrameStep()

class ProcessorInterface:
    def ResetButton_Tap():
        dol_ProcessorInterface.ResetButton_Tap()

class Resources:
    def Init():
        dol_Resources.Resources.Init()
    def GetMisc(id):
        pixmap_addr = <long><void*>new QPixmap_t(dol_Resources.Resources.GetMisc(id))
        return sip.wrapinstance(pixmap_addr, QPixmap)

    BANNER_MISSING = dol_Resources.BANNER_MISSING
    LOGO_LARGE = dol_Resources.LOGO_LARGE
    LOGO_SMALL = dol_Resources.LOGO_SMALL

class State:
    def Save(slot, wait=False):
        dol_State.Save(slot, wait)
    def Load(slot):
        dol_State.Load(slot)
    def SaveAs(fn, wait=False):
        dol_State.SaveAs(fn, wait)
    def LoadAs(fn):
        dol_State.LoadAs(fn)
    def SaveFirstSaved():
        dol_State.SaveFirstSaved()
    def UndoSaveState():
        dol_State.UndoSaveState()
    def UndoLoadState():
        dol_State.UndoLoadState()

class UICommon:
    def SetUserDirectory(dir):
        dol_UICommon.SetUserDirectory(dir)
    def CreateDirectories():
        dol_UICommon.CreateDirectories()
    def Init():
        dol_UICommon.Init()
    def Shutdown():
        dol_UICommon.Shutdown()

_the_host = None

def get_host():
    global _the_host

    if _the_host is None:
        import host
        _the_host = host.Host()

    return _the_host

cdef public void cy_Host_Message(int id) with gil:
    get_host().message(id)

cdef public void cy_Host_UpdateTitle(const string& title) with gil:
    get_host().update_title(bytes(title).decode('ascii'))

cdef public void* cy_Host_GetRenderHandle() with gil:
    return <void*><long>get_host().get_render_handle()

cdef public bool cy_Host_RendererHasFocus() with gil:
    return get_host().get_renderer_has_focus()

cdef public bool cy_Host_RendererIsFullscreen() with gil:
    return get_host().get_renderer_is_fullscreen()

cdef public bool cy_Host_UIHasFocus() with gil:
    return False
