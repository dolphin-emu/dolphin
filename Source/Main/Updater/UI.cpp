// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Updater/UI.h"

#include <CommCtrl.h>

#include <string>

#include "Common/Flag.h"
#include "Common/StringUtil.h"

namespace
{
HWND window_handle = nullptr;
HWND label_handle = nullptr;
HWND progressbar_handle = nullptr;

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

  progressbar_handle = CreateWindow(PROGRESS_CLASS, NULL, PROGRESSBAR_FLAGS, 5, 25, 470, 25,
                                    window_handle, nullptr, nullptr, 0);

  if (!progressbar_handle)
    return false;

  return true;
}

void Destroy()
{
  DestroyWindow(window_handle);
  DestroyWindow(label_handle);
  DestroyWindow(progressbar_handle);
}

void SetMarquee(bool marquee)
{
  SetWindowLong(progressbar_handle, GWL_STYLE, PROGRESSBAR_FLAGS | (marquee ? PBS_MARQUEE : 0));
  SendMessage(progressbar_handle, PBM_SETMARQUEE, marquee, 0);
}

void ResetProgress()
{
  SendMessage(progressbar_handle, PBM_SETPOS, 0, 0);
  SetMarquee(true);
}

void SetProgress(int current, int total)
{
  SendMessage(progressbar_handle, PBM_SETRANGE32, 0, total);
  SendMessage(progressbar_handle, PBM_SETPOS, current, 0);
}

void IncrementProgress(int amount)
{
  SendMessage(progressbar_handle, PBM_DELTAPOS, amount, 0);
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

  SetMarquee(true);

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
