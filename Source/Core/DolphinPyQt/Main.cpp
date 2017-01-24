#include <string>

#include <Python.h>

#include "Common/CommonTypes.h"
#include "Core/Host.h"
#include "wrap_dolphin.h"

PyMODINIT_FUNC PyInit_sip_dolphin(void);

#define ERRCHECK() do { if (PyErr_Occurred()) { PyErr_Print(); return 1; } } while(0)

int main(int argc, char **argv)
{
  wchar_t name[] = L"dolphin-emu-pyqt";

  Py_SetProgramName(name);
  ERRCHECK();
  Py_Initialize();
  ERRCHECK();
  PyRun_SimpleString("import sys; sys.path.insert(0, \"" DATA_DIR "/python\")");
  ERRCHECK();
  PyObject *sys_modules = PyImport_GetModuleDict();
  ERRCHECK();
  PyObject *sip_dolphin = PyInit_sip_dolphin();
  ERRCHECK();
  PyDict_SetItemString(sys_modules, "sip_dolphin", sip_dolphin);
  ERRCHECK();
  PyInit_wrap_dolphin();
  ERRCHECK();
  PyEval_InitThreads();
  ERRCHECK();
  PyRun_SimpleString("import pymain\n"
                     "pymain.main()\n");
  ERRCHECK();
  Py_Finalize();
  ERRCHECK();
  return 0;
}


// Cython's externally-callable functions ("cdef public") are marked
// extern "C".  Use trivial wrappers to call Cython functions for
// Host_* (except that Host_* functions with empty bodies, we don't
// bother to make Cython functions for).

void Host_Message(int id)
{
  cy_Host_Message(id);
}

void Host_UpdateTitle(const std::string& title)
{
  cy_Host_UpdateTitle(title);
}

void* Host_GetRenderHandle()
{
  return cy_Host_GetRenderHandle();
}

bool Host_RendererHasFocus()
{
  return cy_Host_RendererHasFocus();
}

bool Host_RendererIsFullscreen()
{
  return cy_Host_RendererIsFullscreen();
}

bool Host_UIHasFocus()
{
  return cy_Host_UIHasFocus();
}


// Bunches of Host_* functions that do nothing
void Host_UpdateMainFrame()
{
}
void Host_RequestFullscreen(bool enable)
{
}
void Host_RequestRenderWindowSize(int w, int h)
{
}
void Host_NotifyMapLoaded()
{
}
void Host_UpdateDisasmDialog()
{
}
void Host_SetStartupDebuggingParameters()
{
}
void Host_SetWiiMoteConnectionState(int state)
{
}
void Host_ConnectWiimote(int wm_idx, bool connect)
{
}
void Host_ShowVideoConfig(void* parent, const std::string& backend_name)
{
}
void Host_RefreshDSPDebuggerWindow()
{
}
