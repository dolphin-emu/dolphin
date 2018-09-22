// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Updater/UI.h"

#include <CommCtrl.h>
#include <ShObjIdl.h>

#include <string>

#include "Common/Flag.h"
#include "Common/StringUtil.h"

namespace
{
HWND window_handle = nullptr;
HWND label_handle = nullptr;
HWND total_progressbar_handle = nullptr;
HWND current_progressbar_handle = nullptr;
ITaskbarList3* taskbar_list = nullptr;

Common::Flag running;
Common::Flag request_stop;
};  // namespace

constexpr int PROGRESSBAR_FLAGS = WS_VISIBLE | WS_CHILD | PBS_SMOOTH | PBS_SMOOTHREVERSE;

namespace UI
{
bool Init()
{
  InitCommonControls();

  WNDCLASS wndcl = {};
  wndcl.lpfnWndProc = DefWindowProcW;
  wndcl.hbrBackground = GetSysColorBrush(COLOR_MENU);
  wndcl.lpszClassName = L"UPDATER";

  if (!RegisterClass(&wndcl))
    return false;

  window_handle =
      CreateWindow(L"UPDATER", L"Dolphin Updater", WS_VISIBLE | WS_CLIPCHILDREN, CW_USEDEFAULT,
                   CW_USEDEFAULT, 500, 100, nullptr, nullptr, GetModuleHandle(nullptr), 0);

  if (!window_handle)
    return false;

  if (FAILED(CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&taskbar_list))))
  {
    taskbar_list = nullptr;
  }
  if (taskbar_list && FAILED(taskbar_list->HrInit()))
  {
    taskbar_list->Release();
    taskbar_list = nullptr;
  }

  label_handle = CreateWindow(L"STATIC", NULL, WS_VISIBLE | WS_CHILD, 5, 5, 500, 25, window_handle,
                              nullptr, nullptr, 0);

  if (!label_handle)
    return false;

  // Get the default system font
  NONCLIENTMETRICS metrics = {};
  metrics.cbSize = sizeof(NONCLIENTMETRICS);

  if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
    return false;

  SendMessage(label_handle, WM_SETFONT,
              reinterpret_cast<WPARAM>(CreateFontIndirect(&metrics.lfMessageFont)), 0);

  total_progressbar_handle = CreateWindow(PROGRESS_CLASS, NULL, PROGRESSBAR_FLAGS, 5, 25, 470, 25,
                                          window_handle, nullptr, nullptr, 0);

  if (!total_progressbar_handle)
    return false;

  current_progressbar_handle = CreateWindow(PROGRESS_CLASS, NULL, PROGRESSBAR_FLAGS, 5, 30, 470, 25,
                                            window_handle, nullptr, nullptr, 0);

  if (!current_progressbar_handle)
    return false;

  return true;
}

void Destroy()
{
  DestroyWindow(window_handle);
  DestroyWindow(label_handle);
  DestroyWindow(total_progressbar_handle);
  DestroyWindow(current_progressbar_handle);
}

void SetTotalMarquee(bool marquee)
{
  SetWindowLong(total_progressbar_handle, GWL_STYLE,
                PROGRESSBAR_FLAGS | (marquee ? PBS_MARQUEE : 0));
  SendMessage(total_progressbar_handle, PBM_SETMARQUEE, marquee, 0);
  if (taskbar_list)
  {
    taskbar_list->SetProgressState(window_handle, marquee ? TBPF_INDETERMINATE : TBPF_NORMAL);
  }
}

void ResetTotalProgress()
{
  SendMessage(total_progressbar_handle, PBM_SETPOS, 0, 0);
  SetCurrentMarquee(true);
}

void SetTotalProgress(int current, int total)
{
  SendMessage(total_progressbar_handle, PBM_SETRANGE32, 0, total);
  SendMessage(total_progressbar_handle, PBM_SETPOS, current, 0);
  if (taskbar_list)
  {
    taskbar_list->SetProgressValue(window_handle, current, total);
  }
}

void SetCurrentMarquee(bool marquee)
{
  SetWindowLong(current_progressbar_handle, GWL_STYLE,
                PROGRESSBAR_FLAGS | (marquee ? PBS_MARQUEE : 0));
  SendMessage(current_progressbar_handle, PBM_SETMARQUEE, marquee, 0);
}

void ResetCurrentProgress()
{
  SendMessage(current_progressbar_handle, PBM_SETPOS, 0, 0);
  SetCurrentMarquee(true);
}

void Error(const std::string& text)
{
  auto wide_text = UTF8ToUTF16(text);

  MessageBox(nullptr,
             (L"A fatal error occured and the updater cannot continue:\n " + wide_text).c_str(),
             L"Error", MB_ICONERROR);

  if (taskbar_list)
  {
    taskbar_list->SetProgressState(window_handle, TBPF_ERROR);
  }
}

void SetCurrentProgress(int current, int total)
{
  SendMessage(current_progressbar_handle, PBM_SETRANGE32, 0, total);
  SendMessage(current_progressbar_handle, PBM_SETPOS, current, 0);
}

void SetDescription(const std::string& text)
{
  SetWindowText(label_handle, UTF8ToUTF16(text).c_str());
}

void MessageLoop()
{
  request_stop.Clear();
  running.Set();

  if (!Init())
  {
    running.Clear();
    MessageBox(nullptr, L"Window init failed!", L"", MB_ICONERROR);
  }

  SetTotalMarquee(true);
  SetCurrentMarquee(true);

  while (!request_stop.IsSet())
  {
    MSG msg;
    while (PeekMessage(&msg, window_handle, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  running.Clear();

  Destroy();
}

void Stop()
{
  if (!running.IsSet())
    return;

  request_stop.Set();

  while (running.IsSet())
  {
  }
}
};  // namespace UI
