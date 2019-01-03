// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/QtUtils/ScreenSaverInhibitor.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"

#if defined(_WIN32)
#include <Windows.h>
#elif !defined(__APPLE__)
#include <QtDBus/QtDBus>
static constexpr char s_screen_saver_path[] = "/org/freedesktop/ScreenSaver";
static constexpr char s_screen_saver_interface[] = "org.freedesktop.ScreenSaver";
#endif

ScreenSaverInhibitor::ScreenSaverInhibitor(QObject* parent) : QObject(parent)
{
#if defined(_WIN32)
  EXECUTION_STATE es_flags;
  if (SConfig::GetInstance().bDisableScreenSaver)
  {
    NOTICE_LOG(VIDEO, "Inhibiting screen saver.");
    es_flags = ES_DISPLAY_REQUIRED;
  }
  else
  {
    NOTICE_LOG(VIDEO, "Inhibiting system sleep.");
    es_flags = 0;
  }
  SetThreadExecutionState(ES_CONTINUOUS | es_flags | ES_SYSTEM_REQUIRED);

#elif defined(__APPLE__)
  CFStringRef assertion_type;
  if (SConfig::GetInstance().bDisableScreenSaver)
  {
    NOTICE_LOG(VIDEO, "Inhibiting screen saver.");
    assertion_type = kIOPMAssertionTypePreventUserIdleDisplaySleep;
  }
  else
  {
    NOTICE_LOG(VIDEO, "Inhibiting system sleep.");
    assertion_type = kIOPMAssertionTypePreventUserIdleSystemSleep;
  }
  const CFStringRef reason_for_activity = CFSTR("Emulation Running");
  if (IOPMAssertionCreateWithName(assertion_type, kIOPMAssertionLevelOn, reason_for_activity,
                                  &m_power_assertion) != kIOReturnSuccess)
  {
    m_power_assertion = kIOPMNullAssertionID;
  }

#else
  if (SConfig::GetInstance().bDisableScreenSaver)
  {
    NOTICE_LOG(VIDEO, "Inhibiting screen saver.");

    // First attempt using DBus interface for inhibiting screensaver.
    // In situations where the user hasn't installed a screensaver daemon on
    // newer desktop environments (e.g. Arch users on Plasma 5 or GNOME 3),
    // the traditional xdg-screensaver suspend will *not* work. This is
    // theoretically compatible with non-X11 display servers as well.
    auto msg = QDBusMessage::createMethodCall(QLatin1String(s_screen_saver_interface),
                                              QLatin1String(s_screen_saver_path),
                                              QLatin1String(s_screen_saver_interface),
                                              QStringLiteral("Inhibit"));
    msg << QStringLiteral("dolphin-emu") << QStringLiteral("Emulation Running");
    const auto pending_reply = QDBusConnection::sessionBus().asyncCall(msg);
    const auto* call_watcher = new QDBusPendingCallWatcher(pending_reply, this);
    connect(call_watcher, &QDBusPendingCallWatcher::finished, this,
            [this, parent](QDBusPendingCallWatcher* finished_watcher) {
              QDBusPendingReply<uint> reply = *finished_watcher;
              finished_watcher->deleteLater();
              if (!reply.isValid())
              {
                // Fall back to xdg-screensaver.
                // This will likely occur when the DBus interface is not implemented by
                // the user's desktop environment (e.g. MATE and Xfce).
                NOTICE_LOG(VIDEO, "Falling back to xdg-screensaver suspend.");
                if (auto* main_window = qobject_cast<QMainWindow*>(parent))
                {
                  const WId window_id = main_window->winId();
                  if (QProcess::execute(QStringLiteral("xdg-screensaver"),
                                        {QStringLiteral("suspend"),
                                         QStringLiteral("0x%1").arg(window_id, 0, 16)}) < 0)
                    return;
                  m_window_id = window_id;
                }
                return;
              }
              m_cookie = reply.value();
            });
  }

#endif
}

ScreenSaverInhibitor::~ScreenSaverInhibitor()
{
#if defined(_WIN32)
  NOTICE_LOG(VIDEO, "Uninhibiting screen saver.");
  SetThreadExecutionState(ES_CONTINUOUS);

#elif defined(__APPLE__)
  if (m_power_assertion != kIOPMNullAssertionID)
  {
    NOTICE_LOG(VIDEO, "Uninhibiting screen saver.");
    IOPMAssertionRelease(m_power_assertion);
  }

#else
  if (m_cookie != UINT_MAX)
  {
    NOTICE_LOG(VIDEO, "Uninhibiting screen saver.");
    auto msg =
        QDBusMessage::createMethodCall(QLatin1String(s_screen_saver_interface),
                                       QLatin1String(s_screen_saver_path),
                                       QLatin1String(s_screen_saver_interface),
                                       QStringLiteral("UnInhibit"));
    msg << m_cookie;
    QDBusConnection::sessionBus().call(msg, QDBus::NoBlock);
  }
  else if (m_window_id != 0)
  {
    NOTICE_LOG(VIDEO, "Uninhibiting screen saver.");
    QProcess::execute(QStringLiteral("xdg-screensaver"),
                      {QStringLiteral("resume"), QStringLiteral("0x%1").arg(m_window_id, 0, 16)});
  }

#endif
}
