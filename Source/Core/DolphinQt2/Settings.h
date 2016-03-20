// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QSettings>

#include "DiscIO/Volume.h"

// UI settings to be stored in the config directory.
class Settings final : public QSettings
{
	Q_OBJECT

public:
	explicit Settings(QObject* parent = nullptr);

	// UI
	QString GetThemeDir() const;

	// GameList
	QString GetLastGame() const;
	void SetLastGame(const QString& path);
	QStringList GetPaths() const;
	void SetPaths(const QStringList& paths);
	void RemovePath(int i);
	QString GetDefaultGame() const;
	void SetDefaultGame(const QString& path);
	QString GetDVDRoot() const;
	void SetDVDRoot(const QString& path);
	QString GetApploader() const;
	void SetApploader(const QString& path);
	QString GetWiiNAND() const;
	void SetWiiNAND(const QString& path);
	DiscIO::IVolume::ELanguage GetWiiSystemLanguage() const;
	DiscIO::IVolume::ELanguage GetGCSystemLanguage() const;
	bool GetPreferredView() const;
	void SetPreferredView(bool table);

	// Emulation
	bool GetConfirmStop() const;
	int GetStateSlot() const;
	void SetStateSlot(int);

	// Graphics
	bool GetRenderToMain() const;
	bool GetFullScreen() const;
	QSize GetRenderWindowSize() const;
};
