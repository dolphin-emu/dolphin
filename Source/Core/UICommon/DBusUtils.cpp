// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "UICommon/DBusUtils.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QHash>
#include <QObject>
#include <QString>

#include "Common/Logging/Log.h"

namespace DBusUtils
{
static bool InhibitFDO();
static bool InhibitXfce();
static bool InhibitMate();
static bool InhibitPortal();
static void Uninhibit();

static constexpr char s_app_id[] = "org.DolphinEmu.dolphin-emu";

// Cookie for the org.freedesktop.ScreenSaver interface
static u32 s_fdo_cookie = 0;
// Cookie for the org.xfce.ScreenSaver interface
static u32 s_xfce_cookie = 0;
// Cookie for the org.mate.ScreenSaver interface
static u32 s_mate_cookie = 0;
// Return handle for the org.freedesktop.portal.Desktop interface
static QString s_portal_handle;

// Uses D-Bus to inhibit the screensaver
// Tries various D-Bus interfaces until it finds one that works
void InhibitScreenSaver(bool inhibit)
{
  if (inhibit)
  {
    if (s_fdo_cookie || s_xfce_cookie || s_mate_cookie || !s_portal_handle.isEmpty())
      return;
    if (!InhibitFDO() && !InhibitXfce() && !InhibitMate() && !InhibitPortal())
      INFO_LOG_FMT(VIDEO, "Could not inhibit screensaver: No services available");
  }
  else
    Uninhibit();
}

// Inhibits screensaver on Xfce desktop
static bool InhibitXfce()
{
  QDBusInterface interface("org.xfce.ScreenSaver", "/", "org.xfce.ScreenSaver");
  if (!interface.isValid())
    return false;

  QDBusReply<u32> reply = interface.call("Inhibit", s_app_id, QObject::tr("Playing a game"));
  if (interface.lastError().isValid())
  {
    WARN_LOG_FMT(VIDEO, "org.xfce.ScreenSaver::Inhibit failed: {}",
                 interface.lastError().message().toStdString());
    return false;
  }
  INFO_LOG_FMT(VIDEO, "org.xfce.ScreenSaver::Inhibit succeeded");
  s_xfce_cookie = reply;
  return true;
}

// Inhibits screensaver on the MATE desktop
// MATE advertises support for xdg-desktop-portal Inhibit,
// but it doesn't work as of Fedora 40
static bool InhibitMate()
{
  QDBusInterface interface("org.mate.ScreenSaver", "/org/mate/ScreenSaver", "org.mate.ScreenSaver");
  if (!interface.isValid())
    return false;

  QDBusReply<u32> reply = interface.call("Inhibit", s_app_id, QObject::tr("Playing a game"));
  if (interface.lastError().isValid())
  {
    WARN_LOG_FMT(VIDEO, "org.mate.ScreenSaver::Inhibit failed: {}",
                 interface.lastError().message().toStdString());
    return false;
  }
  INFO_LOG_FMT(VIDEO, "org.mate.ScreenSaver::Inhibit succeeded");
  s_mate_cookie = reply;
  return true;
}

// Inhibits screensaver on GNOME, KDE and Cinnamon
static bool InhibitFDO()
{
  QDBusInterface interface("org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
                           "org.freedesktop.ScreenSaver");
  if (!interface.isValid())
    return false;

  QDBusReply<u32> reply = interface.call("Inhibit", s_app_id, QObject::tr("Playing a game"));
  if (interface.lastError().isValid())
  {
    WARN_LOG_FMT(VIDEO, "org.freedesktop.ScreenSaver::Inhibit failed: {}",
                 interface.lastError().message().toStdString());
    return false;
  }
  INFO_LOG_FMT(VIDEO, "org.freedesktop.ScreenSaver::Inhibit succeeded");
  s_fdo_cookie = reply;
  return true;
}

// Inhibits screensaver when sandboxed through Flatpak
// Does not work on KDE Plasma versions before 6.1.5
static bool InhibitPortal()
{
  QDBusInterface interface("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                           "org.freedesktop.portal.Inhibit");
  if (!interface.isValid())
    return false;

  QHash<QString, QVariant> options;
  options["handle_token"] = "dolphin_" + QString::number(std::rand(), 0x10);
  options["reason"] = QObject::tr("Playing a game");
  u32 flags = 9;  // logout | idle
  QDBusReply<QDBusObjectPath> reply = interface.call("Inhibit", "", flags, options);
  if (interface.lastError().isValid())
  {
    WARN_LOG_FMT(VIDEO, "org.freedesktop.portal.Desktop::Inhibit failed: {}",
                 interface.lastError().message().toStdString());
    return false;
  }
  INFO_LOG_FMT(VIDEO, "org.freedesktop.portal.Desktop::Inhibit succeeded");
  s_portal_handle = reply.value().path();
  return true;
}

static void Uninhibit()
{
  if (s_fdo_cookie)
  {
    QDBusInterface interface("org.freedesktop.ScreenSaver", "/org/freedesktop/ScreenSaver",
                             "org.freedesktop.ScreenSaver");
    interface.call("UnInhibit", s_fdo_cookie);
    if (interface.lastError().isValid())
    {
      WARN_LOG_FMT(VIDEO, "org.freedesktop.ScreenSaver::UnInhibit failed: {}",
                   interface.lastError().message().toStdString());
    }
    else
    {
      INFO_LOG_FMT(VIDEO, "org.freedesktop.ScreenSaver::UnInhibit succeeded");
    }
    s_fdo_cookie = 0;
  }

  if (s_xfce_cookie)
  {
    QDBusInterface interface("org.xfce.ScreenSaver", "/org/xfce/ScreenSaver",
                             "org.xfce.ScreenSaver");
    interface.call("UnInhibit", s_xfce_cookie);
    if (interface.lastError().isValid())
    {
      WARN_LOG_FMT(VIDEO, "org.xfce.ScreenSaver::UnInhibit failed: {}",
                   interface.lastError().message().toStdString());
    }
    else
    {
      INFO_LOG_FMT(VIDEO, "org.xfce.ScreenSaver::UnInhibit succeeded");
    }
    s_xfce_cookie = 0;
  }

  if (s_mate_cookie)
  {
    QDBusInterface interface("org.mate.ScreenSaver", "/", "org.mate.ScreenSaver");
    interface.call("UnInhibit", s_mate_cookie);
    if (interface.lastError().isValid())
    {
      WARN_LOG_FMT(VIDEO, "org.mate.ScreenSaver::UnInhibit failed: {}",
                   interface.lastError().message().toStdString());
    }
    else
    {
      INFO_LOG_FMT(VIDEO, "org.mate.ScreenSaver::UnInhibit succeeded");
    }
    s_mate_cookie = 0;
  }

  if (!s_portal_handle.isEmpty())
  {
    QDBusInterface interface("org.freedesktop.portal.Desktop", s_portal_handle,
                             "org.freedesktop.portal.Request");
    interface.call("Close");
    if (interface.lastError().isValid())
    {
      WARN_LOG_FMT(VIDEO, "org.freedesktop.portal.Request::Close failed: {}",
                   interface.lastError().message().toStdString());
    }
    else
    {
      INFO_LOG_FMT(VIDEO, "org.freedesktop.portal.Request::Close succeeded");
    }
    s_portal_handle = QString();
  }
}

}  // namespace DBusUtils
