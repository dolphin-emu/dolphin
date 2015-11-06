// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QApplication>

#include "DolphinQt/MainWindow.h"

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	MainWindow win;
	win.show();
	return app.exec();
}
