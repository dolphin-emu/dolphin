// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "UpdaterCommon/UI.h"

#include <string>
#include <thread>

#include <Windows.h>
#include <CommCtrl.h>
#include <ShObjIdl.h>
#include <shellapi.h>

#include "Common/StringUtil.h"

namespace
{
HWND window_handle = nullptr;
HWND label_handle = nullptr;
HWND total_progressbar_handle = nullptr;
HWND current_progressbar_handle = nullptr;
ITaskbarList3* taskbar_list = nullptr;

std::thread ui_thread;

int GetWindowHeight(HWND hwnd)
{
  RECT rect;
  GetWindowRect(hwnd, &rect);

  return rect.bottom - rect.top;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
};  // namespace

constexpr int PROGRESSBAR_FLAGS = WS_VISIBLE | WS_CHILD | PBS_SMOOTH | PBS_SMOOTHREVERSE;
constexpr int WINDOW_FLAGS = WS_VISIBLE | WS_CLIPCHILDREN;
constexpr int PADDING_HEIGHT = 5;

namespace UI
{
bool InitWindow()
{
  InitCommonControls();

  WNDCLASS wndcl = {};
  wndcl.lpfnWndProc = WindowProc;
  wndcl.hbrBackground = GetSysColorBrush(COLOR_MENU);
  wndcl.lpszClassName = L"UPDATER";

  if (!RegisterClass(&wndcl))
    return false;

  window_handle =
      CreateWindow(L"UPDATER", L"Dolphin Updater", WINDOW_FLAGS, CW_USEDEFAULT, CW_USEDEFAULT, 500,
                   100, nullptr, nullptr, GetModuleHandle(nullptr), 0);

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

  int y = PADDING_HEIGHT;

  label_handle = CreateWindow(L"STATIC", NULL, WS_VISIBLE | WS_CHILD, 5, y, 500, 25, window_handle,
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

  y += GetWindowHeight(label_handle) + PADDING_HEIGHT;

  total_progressbar_handle = CreateWindow(PROGRESS_CLASS, NULL, PROGRESSBAR_FLAGS, 5, y, 470, 25,
                                          window_handle, nullptr, nullptr, 0);

  y += GetWindowHeight(total_progressbar_handle) + PADDING_HEIGHT;

  if (!total_progressbar_handle)
    return false;

  current_progressbar_handle = CreateWindow(PROGRESS_CLASS, NULL, PROGRESSBAR_FLAGS, 5, y, 470, 25,
                                            window_handle, nullptr, nullptr, 0);

  y += GetWindowHeight(current_progressbar_handle) + PADDING_HEIGHT;

  if (!current_progressbar_handle)
    return false;

  RECT rect;
  GetWindowRect(window_handle, &rect);

  // Account for the title bar
  y += GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CYCAPTION) +
       GetSystemMetrics(SM_CXPADDEDBORDER);
  // ...and window border
  y += GetSystemMetrics(SM_CYBORDER);

  // Add some padding for good measure
  y += PADDING_HEIGHT * 3;

  SetWindowPos(window_handle, HWND_TOPMOST, 0, 0, rect.right - rect.left, y, SWP_NOMOVE);

  return true;
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
  if (!InitWindow())
  {
    MessageBox(nullptr, L"Window init failed!", L"", MB_ICONERROR);
    // Destroying the parent (if exists) destroys all children windows
    if (window_handle)
    {
      DestroyWindow(window_handle);
      window_handle = nullptr;
    }
    return;
  }

  SetTotalMarquee(true);
  SetCurrentMarquee(true);

  MSG msg;
  while (GetMessage(&msg, nullptr, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

void Init()
{
  ui_thread = std::thread(MessageLoop);
}

void Stop()
{
  if (window_handle)
    PostMessage(window_handle, WM_CLOSE, 0, 0);

  ui_thread.join();
}

void LaunchApplication(std::string path)
{
  // Hack: Launching the updater over the explorer ensures that admin priviliges are dropped. Why?
  // Ask Microsoft.
  ShellExecuteW(nullptr, nullptr, L"explorer.exe", UTF8ToUTF16(path).c_str(), nullptr, SW_SHOW);
}

void Sleep(int sleep)
{
  ::Sleep(sleep * 1000);
}

void WaitForPID(u32 pid)
{
  HANDLE parent_handle = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
  if (parent_handle)
  {
    WaitForSingleObject(parent_handle, INFINITE);
    CloseHandle(parent_handle);
  }
}

void SetVisible(bool visible)
{
  ShowWindow(window_handle, visible ? SW_SHOW : SW_HIDE);
}

};  // namespace UI
