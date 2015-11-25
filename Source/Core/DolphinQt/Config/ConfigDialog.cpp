// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <QActionGroup>

#include "ui_ConfigDialog.h"

#include "DolphinQt/Config/ConfigDialog.h"
#include "DolphinQt/Utils/Resources.h"
#include "DolphinQt/Utils/Utils.h"

DConfigDialog::DConfigDialog(QWidget* parent_widget)
	: QMainWindow(parent_widget)
{
	setWindowModality(Qt::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// Initial UI setup
	m_ui = std::make_unique<Ui::DConfigDialog>();
	m_ui->setupUi(this);
	UpdateIcons();

	// Create action group 
	QActionGroup* ag = new QActionGroup(this);
	ag->addAction(m_ui->actionPageGeneral);
	ag->addAction(m_ui->actionPageGraphics);
	ag->addAction(m_ui->actionPageAudio);
	ag->addAction(m_ui->actionPageControllers);

	QWidget* leftSpacer = new QWidget(m_ui->toolbar);
	QWidget* rightSpacer = new QWidget(m_ui->toolbar);
	leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	m_ui->toolbar->insertWidget(m_ui->actionPageGeneral, leftSpacer);
	m_ui->toolbar->addWidget(rightSpacer);

	// UI signals/slots
	connect(m_ui->actionPageGeneral, &QAction::triggered, [this]() {
		m_ui->realCentralWidget->setCurrentIndex(0);
	});
	connect(m_ui->actionPageGraphics, &QAction::triggered, [this]() {
		m_ui->realCentralWidget->setCurrentIndex(1);
	});
	connect(m_ui->actionPageAudio, &QAction::triggered, [this]() {
		m_ui->realCentralWidget->setCurrentIndex(2);
	});
	connect(m_ui->actionPageControllers, &QAction::triggered, [this]() {
		m_ui->realCentralWidget->setCurrentIndex(3);
	});
}
DConfigDialog::~DConfigDialog()
{
}

void DConfigDialog::UpdateIcons()
{
	m_ui->actionPageGeneral->setIcon(Resources::GetIcon(Resources::TOOLBAR_CONFIGURE));
	m_ui->actionPageGraphics->setIcon(Resources::GetIcon(Resources::TOOLBAR_GRAPHICS));
	m_ui->actionPageAudio->setIcon(Resources::GetIcon(Resources::TOOLBAR_AUDIO));
	m_ui->actionPageControllers->setIcon(Resources::GetIcon(Resources::TOOLBAR_CONTROLLERS));
}
