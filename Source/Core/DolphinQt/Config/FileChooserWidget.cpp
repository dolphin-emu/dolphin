// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <QFileDialog>
#include <QFileInfo>

#include "ui_FileChooserWidget.h"

#include "DolphinQt/Config/FileChooserWidget.h"

DFileChooser::DFileChooser(QWidget* parent_widget)
	: QWidget(parent_widget)
{
	m_ui = std::make_unique<Ui::DFileChooser>();
	m_ui->setupUi(this);

	connect(m_ui->btnBrowse, &QToolButton::pressed, [this]() -> void {
		QString path;
		if (m_folder)
			path = QFileDialog::getExistingDirectory(this, tr("Choose directory"),
				m_ui->lePath->text());
		else
			path = QFileDialog::getOpenFileName(this, tr("Choose file"),
				QFileInfo(m_ui->lePath->text()).absoluteDir().path(), m_filter);
		if (path.isEmpty())
			return;
		m_ui->lePath->setText(path);
		emit changed();
	});
	connect(m_ui->lePath, &QLineEdit::textChanged, [this]() -> void {
		emit changed();
	});
}
DFileChooser::~DFileChooser()
{
}

void DFileChooser::setPath(const QString& path)
{
	m_ui->lePath->setText(path);
}
QString DFileChooser::path()
{
	return m_ui->lePath->text();
}
