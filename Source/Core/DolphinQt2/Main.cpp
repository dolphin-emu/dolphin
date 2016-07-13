// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QAbstractEventDispatcher>
#include <QApplication>

#include "Core/BootManager.h"
#include "Core/Core.h"
#include "DolphinQt2/Host.h"
#include "DolphinQt2/InDevelopmentWarning.h"
#include "DolphinQt2/MainWindow.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"
#include "UICommon/UICommon.h"

int main(int argc, char* argv[])
{
  QApplication app(argc, argv);

  UICommon::SetUserDirectory("");
  UICommon::CreateDirectories();
  UICommon::Init();
  Resources::Init();

  // Whenever the event loop is about to go to sleep, dispatch the jobs
  // queued in the Core first.
  QObject::connect(QAbstractEventDispatcher::instance(), &QAbstractEventDispatcher::aboutToBlock,
                   &app, &Core::HostDispatchJobs);

  int retval = 0;
  if (Settings().IsInDevelopmentWarningEnabled())
  {
    InDevelopmentWarning warning_box;
    retval = warning_box.exec() == QDialog::Rejected;
  }
  if (!retval)
  {
    MainWindow win;
    win.show();
    retval = app.exec();
  }

  BootManager::Stop();
  Core::Shutdown();
  UICommon::Shutdown();
  Host::GetInstance()->deleteLater();

  return retval;
}
