// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/QtUtils/WinIconHelper.h"

#ifdef _WIN32

#include <Windows.h>

// The following code is adapted from qpixmap_win.cpp (c) The Qt Company Ltd. (https://qt.io)
// Licensed under the GNU GPL v3
static inline BITMAPINFO GetBMI(int width, int height, bool topToBottom)
{
  BITMAPINFO bmi = {};
  auto& bih = bmi.bmiHeader;

  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = width;
  bih.biHeight = topToBottom ? -height : height;
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = width * height * 4;

  return bmi;
}

static QPixmap PixmapFromHICON(HICON icon)
{
  HDC screenDevice = GetDC(0);
  HDC hdc = CreateCompatibleDC(screenDevice);
  ReleaseDC(0, screenDevice);
  ICONINFO iconinfo;
  const bool result = GetIconInfo(icon, &iconinfo);  // x and y Hotspot describes the icon center
  if (!result)
  {
    qErrnoWarning("QPixmap::fromWinHICON(), failed to GetIconInfo()");
    DeleteDC(hdc);
    return QPixmap();
  }
  const int w = iconinfo.xHotspot * 2;
  const int h = iconinfo.yHotspot * 2;
  BITMAPINFO bitmapInfo = GetBMI(w, h, false);
  DWORD* bits;
  HBITMAP winBitmap = CreateDIBSection(hdc, &bitmapInfo, DIB_RGB_COLORS, (VOID**)&bits, NULL, 0);
  HGDIOBJ oldhdc = reinterpret_cast<HBITMAP>(SelectObject(hdc, winBitmap));
  DrawIconEx(hdc, 0, 0, icon, iconinfo.xHotspot * 2, iconinfo.yHotspot * 2, 0, 0, DI_NORMAL);

  QImage image(w, h, QImage::Format_ARGB32_Premultiplied);
  if (image.isNull())
    return {};

  BITMAPINFO bmi = GetBMI(w, h, true);

  QScopedArrayPointer<uchar> data(new uchar[bmi.bmiHeader.biSizeImage]);
  if (!GetDIBits(hdc, winBitmap, 0, h, data.data(), &bmi, DIB_RGB_COLORS))
    return {};

  for (int y = 0; y < image.height(); ++y)
  {
    void* dest = static_cast<void*>(image.scanLine(y));
    const void* src = data.data() + y * image.bytesPerLine();
    memcpy(dest, src, image.bytesPerLine());
  }

  // dispose resources created by iconinfo call
  DeleteObject(iconinfo.hbmMask);
  DeleteObject(iconinfo.hbmColor);
  SelectObject(hdc, oldhdc);  // restore state
  DeleteObject(winBitmap);
  DeleteDC(hdc);
  return QPixmap::fromImage(image);
}

QIcon WinIconHelper::GetNativeIcon()
{
  QIcon icon;
  for (int size : {16, 32, 48, 256})
  {
    HANDLE h = LoadImageW(GetModuleHandleW(nullptr), L"\"DOLPHIN\"", IMAGE_ICON, size, size,
                          LR_CREATEDIBSECTION);

    if (h && h != INVALID_HANDLE_VALUE)
    {
      auto* icon_handle = static_cast<HICON>(h);
      icon.addPixmap(PixmapFromHICON(icon_handle));
    }
  }

  return icon;
}

#endif
