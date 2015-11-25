// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <QActionGroup>

#include "ui_ConfigDialog.h"

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"

#include "DolphinQt/MainWindow.h"
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

	// Populate combos & etc.
	auto sv = DoFileSearch({""}, {
		File::GetUserPath(D_THEMES_IDX),
		File::GetSysDirectory() + THEMES_DIR
	}, /* recursive */ false);
	for (const std::string& filename : sv)
	{
		std::string name, ext;
		SplitPath(filename, nullptr, &name, &ext);
		m_ui->cmbTheme->insertItem(m_ui->cmbTheme->count(), QString::fromStdString(name + ext));
	}

	LoadSettings();

// Helper macros for signal/slot creation
#define SCGI SConfig::GetInstance()
#define cAction(ACTION, CALLBACK) \
	connect(m_ui->ACTION, &QAction::triggered, [this]() -> void CALLBACK)
#define cCombo(COMBO, CALLBACK) \
	connect(m_ui->COMBO, &QComboBox::currentTextChanged, [this]() -> void CALLBACK)
#define cCheck(CHECKBOX, CALLBACK) \
	connect(m_ui->CHECKBOX, &QCheckBox::stateChanged, [this]() -> void CALLBACK)
#define cSpin(SPINBOX, CALLBACK) \
	connect(m_ui->SPINBOX, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this]() -> void CALLBACK)

	// UI signals/slots
	cAction(actionPageGeneral,     { m_ui->realCentralWidget->setCurrentIndex(0); });
	cAction(actionPageGraphics,    { m_ui->realCentralWidget->setCurrentIndex(1); });
	cAction(actionPageAudio,       { m_ui->realCentralWidget->setCurrentIndex(2); });
	cAction(actionPageControllers, { m_ui->realCentralWidget->setCurrentIndex(3); });

	/* Settings signals/slots */
	// General - Basic
	cCheck(chkDualcore, { SCGI.bCPUThread = m_ui->chkDualcore->isChecked();	});
	cCheck(chkIdleSkip, { SCGI.bSkipIdle = m_ui->chkIdleSkip->isChecked(); });
	cCheck(chkCheats,	{ SCGI.bEnableCheats = m_ui->chkCheats->isChecked(); });
	cCombo(cmbFramelimit, {
		unsigned int framelimit = m_ui->cmbFramelimit->currentIndex();
		if (framelimit == 2)
		{
			m_ui->sbFramelimit->setEnabled(true);
			framelimit = (m_ui->sbFramelimit->value() / 5) + 1;
		}
		else
			m_ui->sbFramelimit->setEnabled(false);
		SCGI.m_Framelimit = framelimit;
	});
	cSpin(sbFramelimit, { SCGI.m_Framelimit = (m_ui->sbFramelimit->value() / 5) + 1; });
	// General - Interface
	cCheck(chkConfirmStop,    { SCGI.bConfirmStop = m_ui->chkConfirmStop->isChecked(); });
	cCheck(chkPanicHandlers,  { SCGI.bUsePanicHandlers = m_ui->chkPanicHandlers->isChecked(); });
	cCheck(chkOSDMessages,    { SCGI.bOnScreenDisplayMessages = m_ui->chkOSDMessages->isChecked(); });
	cCheck(chkPauseFocusLost, { SCGI.m_PauseOnFocusLost = m_ui->chkPauseFocusLost->isChecked(); });
	cCombo(cmbTheme, {
		SCGI.theme_name = m_ui->cmbTheme->currentText().toStdString();
		Resources::Init();
		g_main_window->UpdateIcons();
		UpdateIcons();
	});
}
DConfigDialog::~DConfigDialog()
{
}

void DConfigDialog::LoadSettings()
{
	const SConfig& sconf = SConfig::GetInstance();

	// General - Basic
	m_ui->chkDualcore->setChecked(sconf.bCPUThread);
	m_ui->chkIdleSkip->setChecked(sconf.bSkipIdle);
	m_ui->chkCheats->setChecked(sconf.bEnableCheats);
	m_ui->cmbFramelimit->setCurrentIndex(sconf.m_Framelimit);
	if (sconf.m_Framelimit > 1)
	{
		m_ui->cmbFramelimit->setCurrentIndex(2);
		m_ui->sbFramelimit->setEnabled(true);
		m_ui->sbFramelimit->setValue((sconf.m_Framelimit - 1) * 5);
	}
	// General - Interface
	m_ui->chkConfirmStop->setChecked(sconf.bConfirmStop);
	m_ui->chkPanicHandlers->setChecked(sconf.bUsePanicHandlers);
	m_ui->chkOSDMessages->setChecked(sconf.bOnScreenDisplayMessages);
	m_ui->chkPauseFocusLost->setChecked(sconf.m_PauseOnFocusLost);
	m_ui->cmbTheme->setCurrentText(QString::fromStdString(sconf.theme_name));
	// General - Advanced
	//m_force_ntscj_checkbox->setChecked(sconf.bForceNTSCJ);
}

void DConfigDialog::UpdateIcons()
{
	m_ui->actionPageGeneral->setIcon(Resources::GetIcon(Resources::TOOLBAR_CONFIGURE));
	m_ui->actionPageGraphics->setIcon(Resources::GetIcon(Resources::TOOLBAR_GRAPHICS));
	m_ui->actionPageAudio->setIcon(Resources::GetIcon(Resources::TOOLBAR_AUDIO));
	m_ui->actionPageControllers->setIcon(Resources::GetIcon(Resources::TOOLBAR_CONTROLLERS));
}
