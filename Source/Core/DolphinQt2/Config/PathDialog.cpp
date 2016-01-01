// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QAction>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSize>
#include <QVBoxLayout>

#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Config/PathDialog.h"

PathDialog::PathDialog(QWidget* parent)
	: QDialog(parent)
{
	setWindowTitle(tr("Paths"));
	setAttribute(Qt::WA_DeleteOnClose);

	QVBoxLayout* layout = new QVBoxLayout;
	layout->addWidget(MakeGameFolderBox());
	layout->addLayout(MakePathsLayout());

	QDialogButtonBox* ok_box = new QDialogButtonBox(QDialogButtonBox::Ok);
	connect(ok_box, &QDialogButtonBox::accepted, this, &PathDialog::accept);
	layout->addWidget(ok_box);

	setLayout(layout);
}

void PathDialog::Browse()
{
	QString dir = QFileDialog::getExistingDirectory(this,
			tr("Select a Directory"),
			QDir::currentPath());
	if (!dir.isEmpty())
	{
		Settings settings;
		QStringList game_folders = settings.GetPaths();
		if (!game_folders.contains(dir))
		{
			game_folders << dir;
			settings.SetPaths(game_folders);
			m_path_list->addItem(dir);
			emit PathAdded(dir);
		}
	}
}

void PathDialog::BrowseDefaultGame()
{
	QString file = QFileDialog::getOpenFileName(this,
			tr("Select a Game"),
			QDir::currentPath(),
			tr("All GC/Wii files (*.elf *.dol *.gcm *.iso *.wbfs *.ciso *.gcz *.wad);;"
			   "All Files (*)"));
	if (!file.isEmpty())
	{
		m_game_edit->setText(file);
		Settings().SetDefaultGame(file);
	}
}

void PathDialog::BrowseDVDRoot()
{
	QString dir = QFileDialog::getExistingDirectory(this,
			tr("Select DVD Root"),
			QDir::currentPath());
	if (!dir.isEmpty())
	{
		m_dvd_edit->setText(dir);
		Settings().SetDVDRoot(dir);
	}
}

void PathDialog::BrowseApploader()
{
	QString file = QFileDialog::getOpenFileName(this,
			tr("Select an Apploader"),
			QDir::currentPath(),
			tr("Apploaders (*.img)"));
	if (!file.isEmpty())
	{
		m_app_edit->setText(file);
		Settings().SetApploader(file);
	}
}

void PathDialog::BrowseWiiNAND()
{
	QString dir = QFileDialog::getExistingDirectory(this,
			tr("Select Wii NAND Root"),
			QDir::currentPath());
	if (!dir.isEmpty())
	{
		m_nand_edit->setText(dir);
		Settings().SetWiiNAND(dir);
	}
}

QGroupBox* PathDialog::MakeGameFolderBox()
{
	QGroupBox* game_box = new QGroupBox(tr("Game Folders"));
	game_box->setMinimumSize(QSize(400, 250));
	QVBoxLayout* vlayout = new QVBoxLayout;

	m_path_list = new QListWidget;
	m_path_list->insertItems(0, Settings().GetPaths());
	m_path_list->setSpacing(1);
	vlayout->addWidget(m_path_list);

	QHBoxLayout* hlayout = new QHBoxLayout;

	hlayout->addStretch();
	QPushButton* add = new QPushButton(tr("Add"));
	QPushButton* remove = new QPushButton(tr("Remove"));
	hlayout->addWidget(add);
	hlayout->addWidget(remove);
	vlayout->addLayout(hlayout);

	connect(add, &QPushButton::clicked, this, &PathDialog::Browse);
	connect(remove, &QPushButton::clicked, this, &PathDialog::RemovePath);

	game_box->setLayout(vlayout);
	return game_box;
}

QGridLayout* PathDialog::MakePathsLayout()
{
	QGridLayout* layout = new QGridLayout;
	layout->setColumnStretch(1, 1);

	m_game_edit = new QLineEdit(Settings().GetDefaultGame());
	connect(m_game_edit, &QLineEdit::editingFinished,
			[=]{ Settings().SetDefaultGame(m_game_edit->text()); });
	QPushButton* game_open = new QPushButton;
	connect(game_open, &QPushButton::clicked, this, &PathDialog::BrowseDefaultGame);
	layout->addWidget(new QLabel(tr("Default Game")), 0, 0);
	layout->addWidget(m_game_edit, 0, 1);
	layout->addWidget(game_open, 0, 2);

	m_dvd_edit = new QLineEdit(Settings().GetDVDRoot());
	connect(m_dvd_edit, &QLineEdit::editingFinished,
			[=]{ Settings().SetDVDRoot(m_dvd_edit->text()); });
	QPushButton* dvd_open = new QPushButton;
	connect(dvd_open, &QPushButton::clicked, this, &PathDialog::BrowseDVDRoot);
	layout->addWidget(new QLabel(tr("DVD Root")), 1, 0);
	layout->addWidget(m_dvd_edit, 1, 1);
	layout->addWidget(dvd_open, 1, 2);

	m_app_edit = new QLineEdit(Settings().GetApploader());
	connect(m_app_edit, &QLineEdit::editingFinished,
			[=]{ Settings().SetApploader(m_app_edit->text()); });
	QPushButton* app_open = new QPushButton;
	connect(app_open, &QPushButton::clicked, this, &PathDialog::BrowseApploader);
	layout->addWidget(new QLabel(tr("Apploader")), 2, 0);
	layout->addWidget(m_app_edit, 2, 1);
	layout->addWidget(app_open, 2, 2);

	m_nand_edit = new QLineEdit(Settings().GetWiiNAND());
	connect(m_nand_edit, &QLineEdit::editingFinished,
			[=]{ Settings().SetWiiNAND(m_nand_edit->text()); });
	QPushButton* nand_open = new QPushButton;
	connect(nand_open, &QPushButton::clicked, this, &PathDialog::BrowseWiiNAND);
	layout->addWidget(new QLabel(tr("Wii NAND Root")), 3, 0);
	layout->addWidget(m_nand_edit, 3, 1);
	layout->addWidget(nand_open, 3, 2);

	return layout;
}

void PathDialog::RemovePath()
{
	int row = m_path_list->currentRow();
	if (row < 0)
		return;
	emit PathRemoved(m_path_list->takeItem(row)->text());
	Settings().RemovePath(row);
}
