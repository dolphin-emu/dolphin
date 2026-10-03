// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/QtUtils.h"

#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>
#include <QScreen>

#if defined(QT_DBUS_LIB)
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#endif

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace
{

void ShowFolderOfFile(std::string_view file_path)
{
  // Remove everything after the last separator in the game's path, resulting in the parent
  // directory path with a trailing separator. Keeping the trailing separator prevents Windows from
  // mistakenly opening a .bat or .exe file in the grandparent folder when that file has the same
  // base name as the folder (See https://bugs.dolphin-emu.org/issues/12411).
  std::string parent_directory_path;
  SplitPath(file_path, &parent_directory_path, nullptr, nullptr);
  if (parent_directory_path.empty())
  {
    return;
  }

  const QUrl url = QUrl::fromLocalFile(QString::fromStdString(parent_directory_path));
  QDesktopServices::openUrl(url);
}

}  // namespace

namespace QtUtils
{

void ShowFourDigitYear(QDateTimeEdit* widget)
{
  if (!widget->displayFormat().contains(QStringLiteral("yyyy")))
  {
    // Always show the full year, no matter what the locale specifies. Otherwise, two-digit years
    // will always be interpreted as in the 21st century.
    widget->setDisplayFormat(
        widget->displayFormat().replace(QStringLiteral("yy"), QStringLiteral("yyyy")));
  }
}

QWidget* CreateIconWarning(QWidget* parent, QStyle::StandardPixmap standard_pixmap, QLabel* label)
{
  const auto size = QFontMetrics(parent->font()).height() * 5 / 4;

  auto* const icon = new QLabel{};
  icon->setPixmap(parent->style()->standardIcon(standard_pixmap).pixmap(size, size));

  auto* const widget = new QWidget;
  auto* const layout = new QHBoxLayout{widget};
  layout->addWidget(icon);
  layout->addWidget(label, 1);
  return widget;
}

void AdjustSizeWithinScreen(QWidget* widget)
{
  const auto screen_size = widget->screen()->availableSize();

  const auto adj_screen_size = screen_size * 9 / 10;

  widget->resize(widget->sizeHint().boundedTo(adj_screen_size));

  CenterOnParentWindow(widget);
}

void CenterOnParentWindow(QWidget* const widget)
{
  // Find the top-level window.
  const QWidget* const parent_widget{widget->parentWidget()};
  if (!parent_widget)
    return;
  const QWidget* const window{parent_widget->window()};

  // Calculate position based on the widgets' size and position.
  const QRect window_geometry{window->geometry()};
  const QSize window_size{window_geometry.size()};
  const QPoint window_pos{window_geometry.topLeft()};
  const QRect geometry{widget->geometry()};
  const QSize size{geometry.size()};
  const QPoint offset{(window_size.width() - size.width()) / 2,
                      (window_size.height() - size.height()) / 2};
  const QPoint pos{window_pos + offset};

  widget->setGeometry(QRect(pos, size));
}

void ShowFileInFolder(std::string_view file_path)
{
#if defined(QT_DBUS_LIB)
  QDBusInterface dbus{QString::fromLatin1("org.freedesktop.DBus"),
                      QString::fromLatin1("/org/freedesktop/DBus"),
                      QString::fromLatin1("org.freedesktop.DBus")};

  dbus.call(QString::fromLatin1("StartServiceByName"),
            QString::fromLatin1("org.freedesktop.FileManager1"), 0u);

  QDBusInterface iface{QString::fromLatin1("org.freedesktop.FileManager1"),
                       QString::fromLatin1("/org/freedesktop/FileManager1"),
                       QString::fromLatin1("org.freedesktop.FileManager1")};
  if (iface.isValid())
  {
    QStringList urls;
    urls << QUrl::fromLocalFile(QString::fromUtf8(file_path)).toString();

    const QDBusReply<void> reply = iface.call(QString::fromLatin1("ShowItems"), urls, QString{});
    if (reply.isValid())
      return;

    const auto& err = reply.error();
    WARN_LOG_FMT(COMMON, "DBus call failed: {} - {}", err.name().toStdString(),
                 err.message().toStdString());
  }
  else
  {
    WARN_LOG_FMT(COMMON, "Invalid DBus interface: {}", iface.lastError().message().toStdString());
  }

#elif defined(_WIN32)
  if (QProcess{}.startDetached(QString::fromLatin1("explorer.exe"),
                               {QString::fromLatin1("/select"), QString::fromLatin1(","),
                                QDir::toNativeSeparators(QString::fromUtf8(file_path))}))
  {
    return;
  }
#elif defined(__APPLE__)
  if (QProcess{}.startDetached(QString::fromLatin1("open"),
                               {QString::fromLatin1("-R"), QString::fromUtf8(file_path)}))
  {
    return;
  }
#endif

  // Fall back on failure or unsupported platforms.
  ShowFolderOfFile(file_path);
}

}  // namespace QtUtils
