// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <OptionParser.h>
#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QMessageBox>
#include <QObject>

#include "Common/MsgHandler.h"
#include "Core/Analytics.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/Core.h"
#include "DolphinQt2/Host.h"
#include "DolphinQt2/InDevelopmentWarning.h"
#include "DolphinQt2/MainWindow.h"
#include "DolphinQt2/QtUtils/RunOnObject.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Translation.h"
#include "UICommon/CommandLineParse.h"
#include "UICommon/UICommon.h"

static bool QtMsgAlertHandler(const char* caption, const char* text, bool yes_no, MsgType style)
{
  return RunOnObject(QApplication::instance(), [&] {
    QMessageBox message_box(QApplication::activeWindow());
    message_box.setWindowTitle(QString::fromUtf8(caption));
    message_box.setText(QString::fromUtf8(text));
    message_box.setStandardButtons(yes_no ? QMessageBox::Yes | QMessageBox::No : QMessageBox::Ok);
    message_box.setIcon([&] {
      switch (style)
      {
      case MsgType::Information:
        return QMessageBox::Information;
      case MsgType::Question:
        return QMessageBox::Question;
      case MsgType::Warning:
        return QMessageBox::Warning;
      case MsgType::Critical:
        return QMessageBox::Critical;
      }
      // appease MSVC
      return QMessageBox::NoIcon;
    }());

    return message_box.exec() == QMessageBox::Yes;
  });
}

// N.B. On Windows, this should be called from WinMain. Link against qtmain and specify
// /SubSystem:Windows
int main(int argc, char* argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

  QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
  QCoreApplication::setOrganizationName(QStringLiteral("Dolphin Emulator"));
  QCoreApplication::setOrganizationDomain(QStringLiteral("dolphin-emu.org"));
  QCoreApplication::setApplicationName(QStringLiteral("dolphin"));

  QApplication app(argc, argv);

  auto parser = CommandLineParse::CreateParser(CommandLineParse::ParserOptions::IncludeGUIOptions);
  const optparse::Values& options = CommandLineParse::ParseArguments(parser.get(), argc, argv);
  const std::vector<std::string> args = parser->args();

  UICommon::SetUserDirectory(static_cast<const char*>(options.get("user")));
  UICommon::CreateDirectories();
  UICommon::Init();
  Resources::Init();

  // Hook up alerts from core
  RegisterMsgAlertHandler(QtMsgAlertHandler);

  // Hook up translations
  Translation::Initialize();

  // Whenever the event loop is about to go to sleep, dispatch the jobs
  // queued in the Core first.
  QObject::connect(QAbstractEventDispatcher::instance(), &QAbstractEventDispatcher::aboutToBlock,
                   &app, &Core::HostDispatchJobs);

  std::unique_ptr<BootParameters> boot;
  if (options.is_set("nand_title"))
  {
    const std::string hex_string = static_cast<const char*>(options.get("nand_title"));
    if (hex_string.length() == 16)
    {
      const u64 title_id = std::stoull(hex_string, nullptr, 16);
      boot = std::make_unique<BootParameters>(BootParameters::NANDTitle{title_id});
    }
    else
    {
      QMessageBox::critical(nullptr, QObject::tr("Error"), QObject::tr("Invalid title ID."));
    }
  }

  int retval = 0;

  if (SConfig::GetInstance().m_show_development_warning)
  {
    InDevelopmentWarning warning_box;
    retval = warning_box.exec() == QDialog::Rejected;
  }
  if (!retval)
  {
    DolphinAnalytics::Instance()->ReportDolphinStart("qt");

    MainWindow win{std::move(boot)};
    win.show();

#if defined(USE_ANALYTICS) && USE_ANALYTICS
    if (!SConfig::GetInstance().m_analytics_permission_asked)
    {
      QMessageBox analytics_prompt(&win);

      analytics_prompt.setIcon(QMessageBox::Question);
      analytics_prompt.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      analytics_prompt.setText(
          QObject::tr("Do you authorize Dolphin to report information to Dolphin's developers?"));
      analytics_prompt.setInformativeText(
          QObject::tr("If authorized, Dolphin can collect data on its performance, "
                      "feature usage, and configuration, as well as data on your system's "
                      "hardware and operating system.\n\n"
                      "No private data is ever collected. This data helps us understand "
                      "how people and emulated games use Dolphin and prioritize our "
                      "efforts. It also helps us identify rare configurations that are "
                      "causing bugs, performance and stability issues.\n"
                      "This authorization can be revoked at any time through Dolphin's "
                      "settings."));

      const int answer = analytics_prompt.exec();

      SConfig::GetInstance().m_analytics_permission_asked = true;
      SConfig::GetInstance().m_analytics_enabled = (answer == QMessageBox::Yes);

      DolphinAnalytics::Instance()->ReloadConfig();
    }
#endif

    retval = app.exec();
  }

  BootManager::Stop();
  Core::Shutdown();
  UICommon::Shutdown();
  Host::GetInstance()->deleteLater();

  return retval;
}
