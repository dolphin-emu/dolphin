// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QMessageBox>
#include <QSysInfo>
#include <QUrl>

#include "MainWindow.h"

static bool IsOsSupported()
{
#ifdef Q_OS_OSX
	return QSysInfo::MacintoshVersion >= QSysInfo::MV_10_7;
#elif defined(Q_OS_WIN)
	return (QSysInfo::WindowsVersion & QSysInfo::WV_NT_based) >= QSysInfo::WV_VISTA;
#else
	return true;
#endif
}

static QString LowestSupportedOsVersion()
{
#ifdef Q_OS_OSX
	return QStringLiteral("Mac OS X 10.7");
#elif defined(Q_OS_WIN)
	return QStringLiteral("Windows Vista SP2");
#else
	return QStringLiteral("Unknown");
#endif
}

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	// TODO: Add command line options

	if (!IsOsSupported())
	{
		QMessageBox::critical(nullptr, QObject::tr("Unsupported OS"),
		                      QObject::tr("Dolphin requires %1 or greater.\n"
		                                  "Please upgrade to %1 or greater to use Dolphin.")
		                      .arg(LowestSupportedOsVersion()));
		return 1;
	}

	DMainWindow w;
	w.show();

	return app.exec();
}
