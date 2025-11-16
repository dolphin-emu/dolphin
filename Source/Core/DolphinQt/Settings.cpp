// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Settings.h"

#include <atomic>
#include <memory>

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPalette>
#include <QRadioButton>
#include <QSize>
#include <QStyle>
#include <QStyleHints>
#include <QThread>
#include <QWidget>

#include "AudioCommon/AudioCommon.h"

#include "Common/Config/Config.h"
#include "Common/Contains.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/AchievementManager.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/IOS/IOS.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "Core/System.h"

#include "DolphinQt/Host.h"
#include "DolphinQt/QtUtils/QueueOnObject.h"

#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/InputConfig.h"

#include "VideoCommon/NetPlayChatUI.h"
#include "VideoCommon/NetPlayGolfUI.h"

static std::unique_ptr<QPalette> s_default_palette;

Settings::Settings()
{
  qRegisterMetaType<Core::State>();
  m_core_state_changed_hook = Core::AddOnStateChangedCallback([this](Core::State new_state) {
    QueueOnObject(this, [this, new_state] {
      // Avoid signal spam while continuously frame stepping. Will still send a signal for the first
      // and last framestep.
      if (!m_continuously_frame_stepping)
        emit EmulationStateChanged(new_state);
    });
  });

  m_config_changed_callback_id = Config::AddConfigChangedCallback([this] {
    static std::atomic<bool> do_once{true};
    if (do_once.exchange(false))
    {
      // Calling ConfigChanged() with a "delay" can have risks, for example, if from
      // code we change some configs that result in Qt greying out some setting, we could
      // end up editing that setting before its greyed out, sending out an event,
      // which might not be expected or handled by the code, potentially crashing.
      // The only safe option would be to wait on the Qt thread to have finished executing this.
      QueueOnObject(this, [this] {
        do_once = true;
        emit ConfigChanged();
      });
    }
  });

  m_hotplug_event_hook = g_controller_interface.RegisterDevicesChangedCallback([this] {
    if (qApp->thread() == QThread::currentThread())
    {
      emit DevicesChanged();
    }
    else
    {
      // Any device shared_ptr in the host thread needs to be released immediately as otherwise
      // they'd continue living until the queued event has run, but some devices can't be recreated
      // until they are destroyed.
      // This is safe from any thread. Devices will be refreshed and re-acquired and in
      // DevicesChanged(). Calling it without queueing shouldn't cause any deadlocks but is slow.
      emit ReleaseDevices();

      QueueOnObject(this, [this] { emit DevicesChanged(); });
    }
  });
}

Settings::~Settings()
{
  Config::RemoveConfigChangedCallback(m_config_changed_callback_id);
}

void Settings::UnregisterDevicesChangedCallback()
{
  m_hotplug_event_hook.reset();
}

Settings& Settings::Instance()
{
  static Settings settings;
  return settings;
}

QSettings& Settings::GetQSettings()
{
  static QSettings settings(
      QStringLiteral("%1/Qt.ini").arg(QString::fromStdString(File::GetUserPath(D_CONFIG_IDX))),
      QSettings::IniFormat);
  return settings;
}

void Settings::TriggerThemeChanged()
{
  emit ThemeChanged();
}

QString Settings::GetUserStyleName() const
{
  if (GetQSettings().contains(QStringLiteral("userstyle/name")))
    return GetQSettings().value(QStringLiteral("userstyle/name")).toString();

  // Migration code for the old way of storing this setting
  return QFileInfo(GetQSettings().value(QStringLiteral("userstyle/path")).toString()).fileName();
}

void Settings::SetUserStyleName(const QString& stylesheet_name)
{
  GetQSettings().setValue(QStringLiteral("userstyle/name"), stylesheet_name);
}

void Settings::InitDefaultPalette()
{
  s_default_palette = std::make_unique<QPalette>(qApp->palette());
}

bool Settings::IsSystemDark()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  return (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark);
#else
  return false;
#endif
}

bool Settings::IsThemeDark()
{
  return qApp->palette().color(QPalette::Base).valueF() < 0.5;
}

// Calling this before the main window has been created breaks the style of some widgets.
void Settings::ApplyStyle()
{
  const StyleType style_type = GetStyleType();

  {
    const bool use_fusion{style_type == StyleType::FusionLight ||
                          style_type == StyleType::FusionDarkGray ||
                          style_type == StyleType::FusionDark};
    static const QString s_initial_style_name{QApplication::style()->name()};
    const QString style_name{use_fusion ? QStringLiteral("fusion") : s_initial_style_name};
    if (QApplication::style()->name() != style_name)
      QApplication::setStyle(style_name);
  }

  const QString stylesheet_name = GetUserStyleName();
  QString stylesheet_contents;

  // If we haven't found one, we continue with an empty (default) style
  if (!stylesheet_name.isEmpty() && style_type == StyleType::User)
  {
    // Load custom user stylesheet
    QDir directory = QDir(QString::fromStdString(File::GetUserPath(D_STYLES_IDX)));
    QFile stylesheet(directory.filePath(stylesheet_name));

    if (stylesheet.open(QFile::ReadOnly))
      stylesheet_contents = QString::fromUtf8(stylesheet.readAll().data());
  }

  QPalette palette;

  if (style_type == StyleType::FusionLight)
  {
    palette.setColor(QPalette::All, QPalette::Window, QColor(239, 239, 239));
    palette.setColor(QPalette::Disabled, QPalette::Window, QColor(239, 239, 239));
    palette.setColor(QPalette::All, QPalette::WindowText, QColor(0, 0, 0));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(190, 190, 190));
    palette.setColor(QPalette::All, QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(239, 239, 239));
    palette.setColor(QPalette::All, QPalette::AlternateBase, QColor(247, 247, 247));
    palette.setColor(QPalette::Disabled, QPalette::AlternateBase, QColor(247, 247, 247));
    palette.setColor(QPalette::All, QPalette::ToolTipBase, QColor(255, 255, 220));
    palette.setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor(255, 255, 220));
    palette.setColor(QPalette::All, QPalette::ToolTipText, QColor(0, 0, 0));
    palette.setColor(QPalette::Disabled, QPalette::ToolTipText, QColor(0, 0, 0));
    palette.setColor(QPalette::All, QPalette::PlaceholderText, QColor(119, 119, 119));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor(119, 119, 119));
    palette.setColor(QPalette::All, QPalette::Text, QColor(0, 0, 0));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(190, 190, 190));
    palette.setColor(QPalette::All, QPalette::Button, QColor(239, 239, 239));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(239, 239, 239));
    palette.setColor(QPalette::All, QPalette::ButtonText, QColor(0, 0, 0));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(190, 190, 190));
    palette.setColor(QPalette::All, QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::All, QPalette::Light, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::Light, QColor(255, 255, 255));
    palette.setColor(QPalette::All, QPalette::Midlight, QColor(202, 202, 202));
    palette.setColor(QPalette::Disabled, QPalette::Midlight, QColor(202, 202, 202));
    palette.setColor(QPalette::All, QPalette::Dark, QColor(159, 159, 159));
    palette.setColor(QPalette::Disabled, QPalette::Dark, QColor(190, 190, 190));
    palette.setColor(QPalette::All, QPalette::Mid, QColor(184, 184, 184));
    palette.setColor(QPalette::Disabled, QPalette::Mid, QColor(184, 184, 184));
    palette.setColor(QPalette::All, QPalette::Shadow, QColor(118, 118, 118));
    palette.setColor(QPalette::Disabled, QPalette::Shadow, QColor(177, 177, 177));
    palette.setColor(QPalette::All, QPalette::Highlight, QColor(48, 140, 198));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(145, 145, 145));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    palette.setColor(QPalette::All, QPalette::Accent, QColor(48, 140, 198));
    palette.setColor(QPalette::Disabled, QPalette::Accent, QColor(145, 145, 145).darker());
#endif
    palette.setColor(QPalette::All, QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::All, QPalette::Link, QColor(0, 0, 255));
    palette.setColor(QPalette::Disabled, QPalette::Link, QColor(0, 0, 255));
    palette.setColor(QPalette::All, QPalette::LinkVisited, QColor(255, 0, 255));
    palette.setColor(QPalette::Disabled, QPalette::LinkVisited, QColor(255, 0, 255));
  }
  else if (style_type == StyleType::FusionDarkGray)
  {
    palette.setColor(QPalette::All, QPalette::Window, QColor(50, 50, 50));
    palette.setColor(QPalette::Disabled, QPalette::Window, QColor(55, 55, 55));
    palette.setColor(QPalette::All, QPalette::WindowText, QColor(200, 200, 200));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(108, 108, 108));
    palette.setColor(QPalette::All, QPalette::Base, QColor(25, 25, 25));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(30, 30, 30));
    palette.setColor(QPalette::All, QPalette::AlternateBase, QColor(38, 38, 38));
    palette.setColor(QPalette::Disabled, QPalette::AlternateBase, QColor(42, 42, 42));
    palette.setColor(QPalette::All, QPalette::ToolTipBase, QColor(45, 45, 45));
    palette.setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor(45, 45, 45));
    palette.setColor(QPalette::All, QPalette::ToolTipText, QColor(200, 200, 200));
    palette.setColor(QPalette::Disabled, QPalette::ToolTipText, QColor(200, 200, 200));
    palette.setColor(QPalette::All, QPalette::PlaceholderText, QColor(90, 90, 90));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor(90, 90, 90));
    palette.setColor(QPalette::All, QPalette::Text, QColor(200, 200, 200));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(108, 108, 108));
    palette.setColor(QPalette::All, QPalette::Button, QColor(54, 54, 54));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(54, 54, 54));
    palette.setColor(QPalette::All, QPalette::ButtonText, QColor(200, 200, 200));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(108, 108, 108));
    palette.setColor(QPalette::All, QPalette::BrightText, QColor(75, 75, 75));
    palette.setColor(QPalette::Disabled, QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::All, QPalette::Light, QColor(26, 26, 26));
    palette.setColor(QPalette::Disabled, QPalette::Light, QColor(26, 26, 26));
    palette.setColor(QPalette::All, QPalette::Midlight, QColor(40, 40, 40));
    palette.setColor(QPalette::Disabled, QPalette::Midlight, QColor(40, 40, 40));
    palette.setColor(QPalette::All, QPalette::Dark, QColor(108, 108, 108));
    palette.setColor(QPalette::Disabled, QPalette::Dark, QColor(108, 108, 108));
    palette.setColor(QPalette::All, QPalette::Mid, QColor(71, 71, 71));
    palette.setColor(QPalette::Disabled, QPalette::Mid, QColor(71, 71, 71));
    palette.setColor(QPalette::All, QPalette::Shadow, QColor(25, 25, 25));
    palette.setColor(QPalette::Disabled, QPalette::Shadow, QColor(37, 37, 37));
    palette.setColor(QPalette::All, QPalette::Highlight, QColor(45, 140, 225));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(45, 140, 225).darker());
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    palette.setColor(QPalette::All, QPalette::Accent, QColor(45, 140, 225));
    palette.setColor(QPalette::Disabled, QPalette::Accent, QColor(45, 140, 225).darker());
#endif
    palette.setColor(QPalette::All, QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(40, 40, 40));
    palette.setColor(QPalette::All, QPalette::Link, QColor(40, 130, 220));
    palette.setColor(QPalette::Disabled, QPalette::Link, QColor(40, 130, 220).darker());
    palette.setColor(QPalette::All, QPalette::LinkVisited, QColor(110, 70, 150));
    palette.setColor(QPalette::Disabled, QPalette::LinkVisited, QColor(110, 70, 150).darker());
  }
  else if (style_type == StyleType::FusionDark)
  {
    palette.setColor(QPalette::All, QPalette::Window, QColor(22, 22, 22));
    palette.setColor(QPalette::Disabled, QPalette::Window, QColor(30, 30, 30));
    palette.setColor(QPalette::All, QPalette::WindowText, QColor(180, 180, 180));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(90, 90, 90));
    palette.setColor(QPalette::All, QPalette::Base, QColor(35, 35, 35));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(30, 30, 30));
    palette.setColor(QPalette::All, QPalette::AlternateBase, QColor(40, 40, 40));
    palette.setColor(QPalette::Disabled, QPalette::AlternateBase, QColor(35, 35, 35));
    palette.setColor(QPalette::All, QPalette::ToolTipBase, QColor(0, 0, 0));
    palette.setColor(QPalette::Disabled, QPalette::ToolTipBase, QColor(0, 0, 0));
    palette.setColor(QPalette::All, QPalette::ToolTipText, QColor(170, 170, 170));
    palette.setColor(QPalette::Disabled, QPalette::ToolTipText, QColor(170, 170, 170));
    palette.setColor(QPalette::All, QPalette::PlaceholderText, QColor(100, 100, 100));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor(100, 100, 100));
    palette.setColor(QPalette::All, QPalette::Text, QColor(200, 200, 200));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(90, 90, 90));
    palette.setColor(QPalette::All, QPalette::Button, QColor(30, 30, 30));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(20, 20, 20));
    palette.setColor(QPalette::All, QPalette::ButtonText, QColor(180, 180, 180));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(90, 90, 90));
    palette.setColor(QPalette::All, QPalette::BrightText, QColor(75, 75, 75));
    palette.setColor(QPalette::Disabled, QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::All, QPalette::Light, QColor(0, 0, 0));
    palette.setColor(QPalette::Disabled, QPalette::Light, QColor(0, 0, 0));
    palette.setColor(QPalette::All, QPalette::Midlight, QColor(40, 40, 40));
    palette.setColor(QPalette::Disabled, QPalette::Midlight, QColor(40, 40, 40));
    palette.setColor(QPalette::All, QPalette::Dark, QColor(90, 90, 90));
    palette.setColor(QPalette::Disabled, QPalette::Dark, QColor(90, 90, 90));
    palette.setColor(QPalette::All, QPalette::Mid, QColor(60, 60, 60));
    palette.setColor(QPalette::Disabled, QPalette::Mid, QColor(60, 60, 60));
    palette.setColor(QPalette::All, QPalette::Shadow, QColor(10, 10, 10));
    palette.setColor(QPalette::Disabled, QPalette::Shadow, QColor(20, 20, 20));
    palette.setColor(QPalette::All, QPalette::Highlight, QColor(35, 130, 200));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(35, 130, 200).darker());
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    palette.setColor(QPalette::All, QPalette::Accent, QColor(35, 130, 200));
    palette.setColor(QPalette::Disabled, QPalette::Accent, QColor(35, 130, 200).darker());
#endif
    palette.setColor(QPalette::All, QPalette::HighlightedText, QColor(240, 240, 240));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(35, 35, 35));
    palette.setColor(QPalette::All, QPalette::Link, QColor(40, 130, 220));
    palette.setColor(QPalette::Disabled, QPalette::Link, QColor(40, 130, 220).darker());
    palette.setColor(QPalette::All, QPalette::LinkVisited, QColor(110, 70, 150));
    palette.setColor(QPalette::Disabled, QPalette::LinkVisited, QColor(110, 70, 150).darker());
  }
#ifdef _WIN32
  // Unlike other OSes we don't automatically get a default dark theme on Windows.
  // We manually load a dark palette for our included "(Dark)" style,
  //  and for *any* external style when the system is in "Dark" mode.
  // Unfortunately it doesn't seem trivial to load a palette based on the stylesheet itself.
  else if (style_type == StyleType::Dark || (style_type != StyleType::Light && IsSystemDark()))
  {
    if (stylesheet_contents.isEmpty())
    {
      QFile file(QStringLiteral(":/dolphin_dark_win/dark.qss"));
      if (file.open(QFile::ReadOnly))
        stylesheet_contents = QString::fromUtf8(file.readAll().data());
    }

    palette = qApp->style()->standardPalette();
    palette.setColor(QPalette::Window, QColor(32, 32, 32));
    palette.setColor(QPalette::WindowText, QColor(220, 220, 220));
    palette.setColor(QPalette::Base, QColor(32, 32, 32));
    palette.setColor(QPalette::AlternateBase, QColor(48, 48, 48));
    palette.setColor(QPalette::PlaceholderText, QColor(126, 126, 126));
    palette.setColor(QPalette::Text, QColor(220, 220, 220));
    palette.setColor(QPalette::Button, QColor(48, 48, 48));
    palette.setColor(QPalette::ButtonText, QColor(220, 220, 220));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Link, QColor(100, 160, 220));
    palette.setColor(QPalette::LinkVisited, QColor(100, 160, 220));
  }
#endif
  else
  {
    if (s_default_palette)
      palette = *s_default_palette;
  }

  qApp->setPalette(palette);

  // Define tooltips style if not already defined
  if (!stylesheet_contents.contains(QStringLiteral("QToolTip"), Qt::CaseSensitive))
  {
    QColor window_color;
    QColor text_color;
    QColor unused_text_emphasis_color;
    QColor border_color;
    GetToolTipStyle(window_color, text_color, unused_text_emphasis_color, border_color, palette,
                    palette);

    const int padding{QFontMetrics(QFont()).height() / 2};

    const auto tooltip_stylesheet =
        QStringLiteral("QToolTip { background-color: #%1; color: #%2; padding: %3px; "
                       "border: 1px; border-style: solid; border-color: #%4; }")
            .arg(window_color.rgba(), 0, 16)
            .arg(text_color.rgba(), 0, 16)
            .arg(padding)
            .arg(border_color.rgba(), 0, 16);
    stylesheet_contents.append(QStringLiteral("%1").arg(tooltip_stylesheet));
  }

  qApp->setStyleSheet(stylesheet_contents);
}

Settings::StyleType Settings::GetStyleType() const
{
  if (GetQSettings().contains(QStringLiteral("userstyle/styletype")))
  {
    bool ok = false;
    const int type_int = GetQSettings().value(QStringLiteral("userstyle/styletype")).toInt(&ok);
    if (ok && type_int >= static_cast<int>(StyleType::MinValue) &&
        type_int <= static_cast<int>(StyleType::MaxValue))
    {
      return static_cast<StyleType>(type_int);
    }
  }

  // if the style type is unset or invalid, try the old enabled flag instead
  const bool enabled = GetQSettings().value(QStringLiteral("userstyle/enabled"), false).toBool();
  return enabled ? StyleType::User : StyleType::System;
}

void Settings::SetStyleType(StyleType type)
{
  GetQSettings().setValue(QStringLiteral("userstyle/styletype"), static_cast<int>(type));

  // also set the old setting so that the config is correctly intepreted by older Dolphin builds
  GetQSettings().setValue(QStringLiteral("userstyle/enabled"), type == StyleType::User);
}

void Settings::GetToolTipStyle(QColor& window_color, QColor& text_color,
                               QColor& emphasis_text_color, QColor& border_color,
                               const QPalette& palette, const QPalette& high_contrast_palette) const
{
  const auto theme_window_color = palette.color(QPalette::Base);
  const auto theme_window_hsv = theme_window_color.toHsv();
  const auto brightness = theme_window_hsv.value();
  const bool brightness_over_threshold = brightness > 128;
  const QColor emphasis_text_color_1 = Qt::yellow;
  const QColor emphasis_text_color_2 = QColor(QStringLiteral("#0090ff"));  // ~light blue
  if (Config::Get(Config::MAIN_USE_HIGH_CONTRAST_TOOLTIPS))
  {
    window_color = brightness_over_threshold ? QColor(72, 72, 72) : Qt::white;
    text_color = brightness_over_threshold ? Qt::white : Qt::black;
    emphasis_text_color = brightness_over_threshold ? emphasis_text_color_1 : emphasis_text_color_2;
    border_color = high_contrast_palette.color(QPalette::Window).darker(160);
  }
  else
  {
    window_color = palette.color(QPalette::Window);
    text_color = palette.color(QPalette::Text);
    emphasis_text_color = brightness_over_threshold ? emphasis_text_color_2 : emphasis_text_color_1;
    border_color = palette.color(QPalette::Text);
  }
}

QStringList Settings::GetPaths() const
{
  QStringList list;
  for (const auto& path : Config::GetIsoPaths())
    list << QString::fromStdString(path);
  return list;
}

void Settings::AddPath(const QString& qpath)
{
  std::string path = qpath.toStdString();
  std::vector<std::string> paths = Config::GetIsoPaths();

  if (Common::Contains(paths, path))
    return;

  paths.emplace_back(path);
  Config::SetIsoPaths(paths);
  emit PathAdded(qpath);
}

void Settings::RemovePath(const QString& qpath)
{
  std::string path = qpath.toStdString();
  std::vector<std::string> paths = Config::GetIsoPaths();

  if (std::erase(paths, path) == 0)
    return;

  Config::SetIsoPaths(paths);
  emit PathRemoved(qpath);
}

void Settings::RefreshGameList()
{
  emit GameListRefreshRequested();
}

void Settings::NotifyRefreshGameListStarted()
{
  emit GameListRefreshStarted();
}

void Settings::NotifyRefreshGameListComplete()
{
  emit GameListRefreshCompleted();
}

void Settings::NotifyMetadataRefreshComplete()
{
  emit MetadataRefreshCompleted();
}

void Settings::ReloadTitleDB()
{
  emit TitleDBReloadRequested();
}

bool Settings::IsAutoRefreshEnabled() const
{
  return GetQSettings().value(QStringLiteral("gamelist/autorefresh"), true).toBool();
}

void Settings::SetAutoRefreshEnabled(bool enabled)
{
  if (IsAutoRefreshEnabled() == enabled)
    return;

  GetQSettings().setValue(QStringLiteral("gamelist/autorefresh"), enabled);

  emit AutoRefreshToggled(enabled);
}

QString Settings::GetDefaultGame() const
{
  return QString::fromStdString(Config::Get(Config::MAIN_DEFAULT_ISO));
}

void Settings::SetDefaultGame(QString path)
{
  if (GetDefaultGame() != path)
  {
    Config::SetBase(Config::MAIN_DEFAULT_ISO, path.toStdString());
    emit DefaultGameChanged(path);
  }
}

bool Settings::GetPreferredView() const
{
  return GetQSettings().value(QStringLiteral("PreferredView"), true).toBool();
}

void Settings::SetPreferredView(bool list)
{
  GetQSettings().setValue(QStringLiteral("PreferredView"), list);
}

int Settings::GetStateSlot() const
{
  return GetQSettings().value(QStringLiteral("Emulation/StateSlot"), 1).toInt();
}

void Settings::SetStateSlot(int slot)
{
  GetQSettings().setValue(QStringLiteral("Emulation/StateSlot"), slot);
}

Config::ShowCursor Settings::GetCursorVisibility() const
{
  return Config::Get(Config::MAIN_SHOW_CURSOR);
}

bool Settings::GetLockCursor() const
{
  return Config::Get(Config::MAIN_LOCK_CURSOR);
}

void Settings::SetKeepWindowOnTop(bool top)
{
  if (IsKeepWindowOnTopEnabled() == top)
    return;

  emit KeepWindowOnTopChanged(top);
}

bool Settings::IsKeepWindowOnTopEnabled() const
{
  return Config::Get(Config::MAIN_KEEP_WINDOW_ON_TOP);
}

bool Settings::GetGraphicModsEnabled() const
{
  return Config::Get(Config::GFX_MODS_ENABLE);
}

void Settings::SetGraphicModsEnabled(bool enabled)
{
  if (GetGraphicModsEnabled() == enabled)
  {
    return;
  }

  Config::SetBaseOrCurrent(Config::GFX_MODS_ENABLE, enabled);
  emit EnableGfxModsChanged(enabled);
}

int Settings::GetVolume() const
{
  return Config::Get(Config::MAIN_AUDIO_VOLUME);
}

void Settings::SetVolume(int volume)
{
  if (GetVolume() != volume)
    Config::SetBaseOrCurrent(Config::MAIN_AUDIO_VOLUME, volume);
}

void Settings::IncreaseVolume(int volume)
{
  AudioCommon::IncreaseVolume(Core::System::GetInstance(), volume);
}

void Settings::DecreaseVolume(int volume)
{
  AudioCommon::DecreaseVolume(Core::System::GetInstance(), volume);
}

bool Settings::IsLogVisible() const
{
  return GetQSettings().value(QStringLiteral("logging/logvisible")).toBool();
}

void Settings::SetLogVisible(bool visible)
{
  if (IsLogVisible() != visible)
  {
    GetQSettings().setValue(QStringLiteral("logging/logvisible"), visible);
    emit LogVisibilityChanged(visible);
  }
}

bool Settings::IsLogConfigVisible() const
{
  return GetQSettings().value(QStringLiteral("logging/logconfigvisible")).toBool();
}

void Settings::SetLogConfigVisible(bool visible)
{
  if (IsLogConfigVisible() != visible)
  {
    GetQSettings().setValue(QStringLiteral("logging/logconfigvisible"), visible);
    emit LogConfigVisibilityChanged(visible);
  }
}

std::shared_ptr<NetPlay::NetPlayClient> Settings::GetNetPlayClient()
{
  return m_client;
}

void Settings::ResetNetPlayClient(NetPlay::NetPlayClient* client)
{
  m_client.reset(client);

  g_netplay_chat_ui.reset();
  g_netplay_golf_ui.reset();
}

std::shared_ptr<NetPlay::NetPlayServer> Settings::GetNetPlayServer()
{
  return m_server;
}

void Settings::ResetNetPlayServer(NetPlay::NetPlayServer* server)
{
  m_server.reset(server);
}

bool Settings::GetCheatsEnabled() const
{
  return Config::Get(Config::MAIN_ENABLE_CHEATS);
}

void Settings::SetDebugModeEnabled(bool enabled)
{
  if (AchievementManager::GetInstance().IsHardcoreModeActive())
    enabled = false;
  if (IsDebugModeEnabled() != enabled)
  {
    Config::SetBaseOrCurrent(Config::MAIN_ENABLE_DEBUGGING, enabled);
    emit DebugModeToggled(enabled);
  }
}

bool Settings::IsDebugModeEnabled() const
{
  return Config::Get(Config::MAIN_ENABLE_DEBUGGING);
}

void Settings::SetRegistersVisible(bool enabled)
{
  if (IsRegistersVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showregisters"), enabled);

    emit RegistersVisibilityChanged(enabled);
  }
}

bool Settings::IsThreadsVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showthreads")).toBool();
}

void Settings::SetThreadsVisible(bool enabled)
{
  if (IsThreadsVisible() == enabled)
    return;

  GetQSettings().setValue(QStringLiteral("debugger/showthreads"), enabled);
  emit ThreadsVisibilityChanged(enabled);
}

bool Settings::IsRegistersVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showregisters")).toBool();
}

void Settings::SetWatchVisible(bool enabled)
{
  if (IsWatchVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showwatch"), enabled);

    emit WatchVisibilityChanged(enabled);
  }
}

bool Settings::IsWatchVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showwatch")).toBool();
}

void Settings::SetBreakpointsVisible(bool enabled)
{
  if (IsBreakpointsVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showbreakpoints"), enabled);

    emit BreakpointsVisibilityChanged(enabled);
  }
}

bool Settings::IsBreakpointsVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showbreakpoints")).toBool();
}

void Settings::SetCodeVisible(bool enabled)
{
  if (IsCodeVisible() != enabled)
  {
    GetQSettings().setValue(QStringLiteral("debugger/showcode"), enabled);

    emit CodeVisibilityChanged(enabled);
  }
}

bool Settings::IsCodeVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showcode"), true).toBool();
}

void Settings::SetMemoryVisible(bool enabled)
{
  if (IsMemoryVisible() == enabled)
    return;
  GetQSettings().setValue(QStringLiteral("debugger/showmemory"), enabled);

  emit MemoryVisibilityChanged(enabled);
}

bool Settings::IsMemoryVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showmemory")).toBool();
}

void Settings::SetNetworkVisible(bool enabled)
{
  if (IsNetworkVisible() == enabled)
    return;

  GetQSettings().setValue(QStringLiteral("debugger/shownetwork"), enabled);
  emit NetworkVisibilityChanged(enabled);
}

bool Settings::IsNetworkVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/shownetwork")).toBool();
}

void Settings::SetJITVisible(bool enabled)
{
  if (IsJITVisible() == enabled)
    return;
  GetQSettings().setValue(QStringLiteral("debugger/showjit"), enabled);

  emit JITVisibilityChanged(enabled);
}

bool Settings::IsJITVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showjit")).toBool();
}

void Settings::SetAssemblerVisible(bool enabled)
{
  if (IsAssemblerVisible() == enabled)
    return;
  GetQSettings().setValue(QStringLiteral("debugger/showassembler"), enabled);

  emit AssemblerVisibilityChanged(enabled);
}

bool Settings::IsAssemblerVisible() const
{
  return GetQSettings().value(QStringLiteral("debugger/showassembler")).toBool();
}

void Settings::RefreshWidgetVisibility()
{
  emit DebugModeToggled(IsDebugModeEnabled());
  emit LogVisibilityChanged(IsLogVisible());
  emit LogConfigVisibilityChanged(IsLogConfigVisible());
}

void Settings::SetDebugFont(QFont font)
{
  if (GetDebugFont() != font)
  {
    GetQSettings().setValue(QStringLiteral("debugger/font"), font);

    emit DebugFontChanged(font);
  }
}

QFont Settings::GetDebugFont() const
{
  QFont default_font = QFont(QFontDatabase::systemFont(QFontDatabase::FixedFont).family());
  default_font.setPointSizeF(9.0);

  return GetQSettings().value(QStringLiteral("debugger/font"), default_font).value<QFont>();
}

void Settings::SetAutoUpdateTrack(const QString& mode)
{
  if (mode == GetAutoUpdateTrack())
    return;

  Config::SetBase(Config::MAIN_AUTOUPDATE_UPDATE_TRACK, mode.toStdString());

  emit AutoUpdateTrackChanged(mode);
}

QString Settings::GetAutoUpdateTrack() const
{
  return QString::fromStdString(Config::Get(Config::MAIN_AUTOUPDATE_UPDATE_TRACK));
}

void Settings::SetFallbackRegion(const DiscIO::Region& region)
{
  if (region == GetFallbackRegion())
    return;

  Config::SetBase(Config::MAIN_FALLBACK_REGION, region);

  emit FallbackRegionChanged(region);
}

DiscIO::Region Settings::GetFallbackRegion() const
{
  return Config::Get(Config::MAIN_FALLBACK_REGION);
}

void Settings::SetAnalyticsEnabled(bool enabled)
{
  if (enabled == IsAnalyticsEnabled())
    return;

  Config::SetBase(Config::MAIN_ANALYTICS_ENABLED, enabled);

  emit AnalyticsToggled(enabled);
}

bool Settings::IsAnalyticsEnabled() const
{
  return Config::Get(Config::MAIN_ANALYTICS_ENABLED);
}

void Settings::SetToolBarVisible(bool visible)
{
  if (IsToolBarVisible() == visible)
    return;

  GetQSettings().setValue(QStringLiteral("toolbar/visible"), visible);

  emit ToolBarVisibilityChanged(visible);
}

bool Settings::IsToolBarVisible() const
{
  return GetQSettings().value(QStringLiteral("toolbar/visible"), true).toBool();
}

void Settings::SetWidgetsLocked(bool locked)
{
  if (AreWidgetsLocked() == locked)
    return;

  GetQSettings().setValue(QStringLiteral("widgets/locked"), locked);

  emit WidgetLockChanged(locked);
}

bool Settings::AreWidgetsLocked() const
{
  return GetQSettings().value(QStringLiteral("widgets/locked"), true).toBool();
}

bool Settings::IsBatchModeEnabled() const
{
  return m_batch;
}
void Settings::SetBatchModeEnabled(bool batch)
{
  m_batch = batch;
}

bool Settings::IsSDCardInserted() const
{
  return Config::Get(Config::MAIN_WII_SD_CARD);
}

void Settings::SetSDCardInserted(bool inserted)
{
  if (IsSDCardInserted() != inserted)
    Config::SetBaseOrCurrent(Config::MAIN_WII_SD_CARD, inserted);
}

bool Settings::IsUSBKeyboardConnected() const
{
  return Config::Get(Config::MAIN_WII_KEYBOARD);
}

void Settings::SetUSBKeyboardConnected(bool connected)
{
  if (IsUSBKeyboardConnected() != connected)
    Config::SetBaseOrCurrent(Config::MAIN_WII_KEYBOARD, connected);
}

bool Settings::IsWiiSpeakMuted() const
{
  return Config::Get(Config::MAIN_WII_SPEAK_MUTED);
}

void Settings::SetWiiSpeakMuted(bool muted)
{
  if (IsWiiSpeakMuted() == muted)
    return;

  Config::SetBaseOrCurrent(Config::MAIN_WII_SPEAK_MUTED, muted);
  emit WiiSpeakMuteChanged(muted);
}

void Settings::SetIsContinuouslyFrameStepping(bool is_stepping)
{
  m_continuously_frame_stepping = is_stepping;
}

bool Settings::GetIsContinuouslyFrameStepping() const
{
  return m_continuously_frame_stepping;
}
