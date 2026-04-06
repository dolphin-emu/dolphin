// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(_WIN32)

#include "DolphinQt/QtUtils/SetWindowDecorations.h"

#include <QApplication>
#include <QEvent>
#include <QWidget>

#include <dwmapi.h>

#include "DolphinQt/Settings.h"

namespace
{

void SetQWidgetWindowDecorations(QWidget* widget)
{
  if (!Settings::Instance().IsThemeDark())
    return;

  constexpr DWORD attribute = 20;  // DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE
  constexpr BOOL use_dark_title_bar = TRUE;

  DwmSetWindowAttribute(HWND(widget->winId()), attribute, &use_dark_title_bar,
                        DWORD(sizeof(use_dark_title_bar)));
}

class WindowDecorationFilter final : public QObject
{
public:
  using QObject::QObject;

  bool eventFilter(QObject* obj, QEvent* event) override
  {
    if (event->type() == QEvent::Show || event->type() == QEvent::ApplicationPaletteChange)
    {
      SetQWidgetWindowDecorations(static_cast<QWidget*>(obj));
    }
    return QObject::eventFilter(obj, event);
  }
};

class WindowDecorationFilterInstaller final : public QObject
{
public:
  using QObject::QObject;

  bool eventFilter(QObject* obj, QEvent* event) override
  {
    if (event->type() == QEvent::WinIdChange)
    {
      auto* const widget = qobject_cast<QWidget*>(obj);

      // Install the real filter on actual Window QWidgets.
      // This avoids the need for a qobject_cast on literally every QEvent::Show.
      if (widget && widget->isWindow())
        widget->installEventFilter(m_decoration_filter);
    }
    return QObject::eventFilter(obj, event);
  }

private:
  QObject* const m_decoration_filter = new WindowDecorationFilter{this};
};

}  // namespace

namespace QtUtils
{

void InstallWindowDecorationFilter(QApplication* app)
{
  app->installEventFilter(new WindowDecorationFilterInstaller{app});
}

}  // namespace QtUtils

#endif
