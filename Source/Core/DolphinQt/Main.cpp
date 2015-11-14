// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QApplication>

#include "Core/BootManager.h"
#include "Core/Core.h"
#include "DolphinQt/MainWindow.h"
#include "DolphinQt/Resources.h"
#include "UICommon/UICommon.h"

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	UICommon::SetUserDirectory("");
	UICommon::CreateDirectories();
	UICommon::Init();
	Resources::Init();

	MainWindow win;
	win.show();
	int retval = app.exec();

	BootManager::Stop();
	Core::Shutdown();
	UICommon::Shutdown();

	return retval;
}
