// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32
#include <cstdio>
#include <string>
#include <vector>

#include <Windows.h>
#endif

#ifdef __linux__
#include <cstdlib>
#endif

#include <OptionParser.h>
#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QObject>
#include <QPushButton>
#include <QWidget>

#include "Common/Config/Config.h"
#include "Common/MsgHandler.h"
#include "Common/ScopeGuard.h"

#include "Core/Boot/Boot.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"
#include "Core/DolphinAnalytics.h"
#include "Core/System.h"

#include "DolphinQt/Host.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/RunOnObject.h"
#include "DolphinQt/QtUtils/SetWindowDecorations.h"
#include "DolphinQt/Resources.h"
#include "DolphinQt/Settings.h"
#include "DolphinQt/Translation.h"
#include "DolphinQt/Updater.h"

#include "UICommon/CommandLineParse.h"
#include "UICommon/UICommon.h"

static bool QtMsgAlertHandler(const char* caption, const char* text, bool yes_no,
                              Common::MsgType style)
{
  const bool called_from_cpu_thread = Core::IsCPUThread();
  const bool called_from_gpu_thread = Core::IsGPUThread();

  std::optional<bool> r = RunOnObject(QApplication::instance(), [&] {
    // If we were called from the CPU/GPU thread, set us as the CPU/GPU thread.
    // This information is used in order to avoid deadlocks when calling e.g.
    // Host::SetRenderFocus or Core::CPUThreadGuard. (Host::SetRenderFocus
    // can get called automatically when a dialog steals the focus.)

    Common::ScopeGuard cpu_scope_guard(&Core::UndeclareAsCPUThread);
    Common::ScopeGuard gpu_scope_guard(&Core::UndeclareAsGPUThread);

    if (!called_from_cpu_thread)
      cpu_scope_guard.Dismiss();
    if (!called_from_gpu_thread)
      gpu_scope_guard.Dismiss();

    if (called_from_cpu_thread)
      Core::DeclareAsCPUThread();
    if (called_from_gpu_thread)
      Core::DeclareAsGPUThread();

    ModalMessageBox message_box(QApplication::activeWindow(), Qt::ApplicationModal);
    message_box.setWindowTitle(QString::fromUtf8(caption));
    message_box.setText(QString::fromUtf8(text));

    message_box.setStandardButtons(yes_no ? QMessageBox::Yes | QMessageBox::No : QMessageBox::Ok);
    if (style == Common::MsgType::Warning)
      message_box.addButton(QMessageBox::Ignore)->setText(QObject::tr("Ignore for this session"));

    message_box.setIcon([&] {
      switch (style)
      {
      case Common::MsgType::Information:
        return QMessageBox::Information;
      case Common::MsgType::Question:
        return QMessageBox::Question;
      case Common::MsgType::Warning:
        return QMessageBox::Warning;
      case Common::MsgType::Critical:
        return QMessageBox::Critical;
      }
      // appease MSVC
      return QMessageBox::NoIcon;
    }());

    SetQWidgetWindowDecorations(&message_box);
    const int button = message_box.exec();
    if (button == QMessageBox::Yes)
      return true;

    if (button == QMessageBox::Ignore)
    {
      Config::SetCurrent(Config::MAIN_USE_PANIC_HANDLERS, false);
      return true;
    }

    return false;
  });
  if (r.has_value())
    return *r;
  return false;
}

#ifdef _WIN32
#define main app_main
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
  const bool console_attached = AttachConsole(ATTACH_PARENT_PROCESS) != FALSE;
  HANDLE stdout_handle = ::GetStdHandle(STD_OUTPUT_HANDLE);
  if (console_attached && stdout_handle)
  {
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
  }
#endif

  Core::DeclareAsHostThread();

#ifdef __APPLE__
  // On macOS, a command line option matching the format "-psn_X_XXXXXX" is passed when
  // the application is launched for the first time. This is to set the "ProcessSerialNumber",
  // something used by the legacy Process Manager from Carbon. optparse will fail if it finds
  // this as it isn't a valid Dolphin command line option, so pretend like it doesn't exist
  // if found.
  if (strncmp(argv[argc - 1], "-psn", 4) == 0)
  {
    argc--;
  }
#endif

#ifdef __linux__
  // Qt 6.3+ has a bug which causes mouse inputs to not be registered in our XInput2 code.
  // If we define QT_XCB_NO_XI2, Qt's xcb platform plugin no longer initializes its XInput
  // code, which makes mouse inputs work again.
  // For more information: https://bugs.dolphin-emu.org/issues/12913
#if (QT_VERSION >= QT_VERSION_CHECK(6, 3, 0))
  setenv("QT_XCB_NO_XI2", "1", true);
#endif
#endif

  QCoreApplication::setOrganizationName(QStringLiteral("Dolphin Emulator"));
  QCoreApplication::setOrganizationDomain(QStringLiteral("dolphin-emu.org"));
  QCoreApplication::setApplicationName(QStringLiteral("dolphin-emu"));

  // QApplication will parse arguments and remove any it recognizes as targeting Qt
  QApplication app(argc, argv);

  auto parser = CommandLineParse::CreateParser(CommandLineParse::ParserOptions::IncludeGUIOptions);
  const optparse::Values& options = CommandLineParse::ParseArguments(parser.get(), argc, argv);
  const std::vector<std::string> args = parser->args();

#ifdef _WIN32
  FreeConsole();
#endif

  UICommon::SetUserDirectory(static_cast<const char*>(options.get("user")));
  UICommon::CreateDirectories();
  UICommon::Init();
  Resources::Init();
  Settings::Instance().SetBatchModeEnabled(options.is_set("batch"));

  // Hook up alerts from core
  Common::RegisterMsgAlertHandler(QtMsgAlertHandler);

  // Hook up translations
  Translation::Initialize();

  // Whenever the event loop is about to go to sleep, dispatch the jobs
  // queued in the Core first.
  QObject::connect(QAbstractEventDispatcher::instance(), &QAbstractEventDispatcher::aboutToBlock,
                   &app, [] { Core::HostDispatchJobs(Core::System::GetInstance()); });

  std::optional<std::string> save_state_path;
  if (options.is_set("save_state"))
  {
    save_state_path = static_cast<const char*>(options.get("save_state"));
  }

  std::unique_ptr<BootParameters> boot;
  bool game_specified = false;
  if (options.is_set("exec"))
  {
    const std::list<std::string> paths_list = options.all("exec");
    const std::vector<std::string> paths{std::make_move_iterator(std::begin(paths_list)),
                                         std::make_move_iterator(std::end(paths_list))};
    boot = BootParameters::GenerateFromFile(
        paths, BootSessionData(save_state_path, DeleteSavestateAfterBoot::No));
    game_specified = true;
  }
  else if (options.is_set("nand_title"))
  {
    const std::string hex_string = static_cast<const char*>(options.get("nand_title"));
    if (hex_string.length() == 16)
    {
      const u64 title_id = std::stoull(hex_string, nullptr, 16);
      boot = std::make_unique<BootParameters>(BootParameters::NANDTitle{title_id});
    }
    else
    {
      ModalMessageBox::critical(nullptr, QObject::tr("Error"), QObject::tr("Invalid title ID."));
    }
    game_specified = true;
  }
  else if (!args.empty())
  {
    boot = BootParameters::GenerateFromFile(
        args.front(), BootSessionData(save_state_path, DeleteSavestateAfterBoot::No));
    game_specified = true;
  }

  int retval;

  if (save_state_path && !game_specified)
  {
    ModalMessageBox::critical(
        nullptr, QObject::tr("Error"),
        QObject::tr("A save state cannot be loaded without specifying a game to launch."));
    retval = 1;
  }
  else if (Settings::Instance().IsBatchModeEnabled() && !game_specified)
  {
    ModalMessageBox::critical(
        nullptr, QObject::tr("Error"),
        QObject::tr("Batch mode cannot be used without specifying a game to launch."));
    retval = 1;
  }
  else if (!boot && (Settings::Instance().IsBatchModeEnabled() || save_state_path))
  {
    // A game to launch was specified, but it was invalid.
    // An error has already been shown by code above, so exit without showing another error.
    retval = 1;
  }
  else
  {
    DolphinAnalytics::Instance().ReportDolphinStart("qt");

    Settings::Instance().InitDefaultPalette();
    Settings::Instance().UpdateSystemDark();
    Settings::Instance().ApplyStyle();

    MainWindow win{std::move(boot), static_cast<const char*>(options.get("movie"))};
    win.Show();

#if defined(USE_ANALYTICS) && USE_ANALYTICS
    if (!Config::Get(Config::MAIN_ANALYTICS_PERMISSION_ASKED))
    {
      ModalMessageBox analytics_prompt(&win);

      analytics_prompt.setIcon(QMessageBox::Question);
      analytics_prompt.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
      analytics_prompt.setWindowTitle(QObject::tr("Allow Usage Statistics Reporting"));
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

      SetQWidgetWindowDecorations(&analytics_prompt);
      const int answer = analytics_prompt.exec();

      Config::SetBase(Config::MAIN_ANALYTICS_PERMISSION_ASKED, true);
      Settings::Instance().SetAnalyticsEnabled(answer == QMessageBox::Yes);

      DolphinAnalytics::Instance().ReloadConfig();
    }
#endif

    if (!Settings::Instance().IsBatchModeEnabled())
    {
      auto* updater = new Updater(&win, Config::Get(Config::MAIN_AUTOUPDATE_UPDATE_TRACK),
                                  Config::Get(Config::MAIN_AUTOUPDATE_HASH_OVERRIDE));
      updater->start();
    }

    retval = app.exec();
  }

  Core::Shutdown(Core::System::GetInstance());
  UICommon::Shutdown();
  Host::GetInstance()->deleteLater();

  return retval;
}

#ifdef _WIN32
int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
  std::vector<std::string> args = Common::CommandLineToUtf8Argv(GetCommandLineW());
  const int argc = static_cast<int>(args.size());
  std::vector<char*> argv(args.size());
  for (size_t i = 0; i < args.size(); ++i)
    argv[i] = args[i].data();

  return main(argc, argv.data());
}

#undef main
#endif
