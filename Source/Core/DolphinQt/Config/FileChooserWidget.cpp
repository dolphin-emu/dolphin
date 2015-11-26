// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <QFileDialog>

#include "ui_FileChooserWidget.h"

#include "DolphinQt/Config/FileChooserWidget.h"

DFileChooser::DFileChooser(QWidget* parent_widget)
	: QWidget(parent_widget)
{
	m_ui = std::make_unique<Ui::DFileChooser>();
	m_ui->setupUi(this);

	connect(m_ui->btnBrowse, &QToolButton::pressed, [this]() -> void {
		QString path = QFileDialog::getOpenFileName(this, tr("Choose path"),
			QStringLiteral(""), QStringLiteral("All files (*)"));
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

void DFileChooser::setPath(QString path)
{
	m_ui->lePath->setText(path);
}
QString DFileChooser::path()
{
	return m_ui->lePath->text();
}
