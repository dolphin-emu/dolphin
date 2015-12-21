// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QSettings>

#include "DiscIO/Volume.h"

class Settings final : public QSettings
{
	Q_OBJECT

public:
	Settings(QObject* parent = nullptr);

	// UI
	QString GetThemeDir() const;

	// GameList
	QString GetLastGame() const;
	void SetLastGame(QString path);
	QStringList GetPaths() const;
	void SetPaths(QStringList paths);
	DiscIO::IVolume::ELanguage GetWiiSystemLanguage() const;
	DiscIO::IVolume::ELanguage GetGCSystemLanguage() const;

	// Emulation
	bool GetConfirmStop() const;

	// Graphics
	bool GetRenderToMain() const;
	bool GetFullScreen() const;
	QSize GetRenderWindowSize() const;
};
