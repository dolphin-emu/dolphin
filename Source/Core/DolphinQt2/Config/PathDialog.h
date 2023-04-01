// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QListWidget>

class PathDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit PathDialog(QWidget* parent = nullptr);

public slots:
	void Browse();
	void BrowseDefaultGame();
	void BrowseDVDRoot();
	void BrowseApploader();
	void BrowseWiiNAND();

signals:
	void PathAdded(QString path);
	void PathRemoved(QString path);

private:
	QGroupBox* MakeGameFolderBox();
	QGridLayout* MakePathsLayout();
	void RemovePath();

	QListWidget* m_path_list;
	QLineEdit* m_game_edit;
	QLineEdit* m_dvd_edit;
	QLineEdit* m_app_edit;
	QLineEdit* m_nand_edit;
};
