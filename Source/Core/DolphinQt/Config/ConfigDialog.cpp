// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <QActionGroup>
#include <QSysInfo>
#include <windows.h>

#include "ui_ConfigDialog.h"

#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinQt/MainWindow.h"
#include "DolphinQt/Config/ConfigDialog.h"
#include "DolphinQt/Utils/Resources.h"

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

#ifdef Q_OS_WIN
	// "Unified titlebar and toolbar" effect
	if (QSysInfo::WindowsVersion == QSysInfo::WV_WINDOWS10)
	{
		QPalette pal = m_ui->toolbar->palette();
		pal.setColor(QPalette::Button, Qt::white);
		m_ui->toolbar->setPalette(pal);
	}
#endif

	// Populate combos (that don't change) & etc.
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

	InitStaticData();
	LoadSettings();
	SetupSlots();
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

// Static data (translation-specific mappings to enums, etc.)
static QMap<int, QString> s_cpu_engines;

void DConfigDialog::InitStaticData()
{
	s_cpu_engines = {
		 { PowerPC::CORE_INTERPRETER, tr("Interpreter (slowest)") },
		 { PowerPC::CORE_CACHEDINTERPRETER, tr("Cached Interpreter (slower)") },
		 #ifdef _M_X86_64
		 { PowerPC::CORE_JIT64, tr("JIT Recompiler (recommended)") },
		 { PowerPC::CORE_JITIL64, tr("JITIL Recompiler (slow, experimental)") },
		 #elif defined(_M_ARM_64)
		 { PowerPC::CORE_JITARM64, tr("JIT Arm64 (experimental)") },
		 #endif
	};
	m_ui->cmbCpuEngine->clear();
	for (QString str : s_cpu_engines.values())
		m_ui->cmbCpuEngine->insertItem(m_ui->cmbCpuEngine->count(), str);
}

void DConfigDialog::SetupSlots()
{
	// Helper macros for signal/slot creation
#define SCGI SConfig::GetInstance()
#define cAction(ACTION, CALLBACK) \
	connect(m_ui->ACTION, &QAction::triggered, [this]() -> void CALLBACK)
#define cCombo(COMBO, CALLBACK) \
	connect(m_ui->COMBO, &QComboBox::currentTextChanged, [this]() -> void CALLBACK)
#define cCheck(CHECKBOX, CALLBACK) \
	connect(m_ui->CHECKBOX, &QCheckBox::stateChanged, [this]() -> void CALLBACK)
#define cGbCheck(CHECKBOX, CALLBACK) \
	connect(m_ui->CHECKBOX, &QGroupBox::toggled, [this]() -> void CALLBACK)
#define cSpin(SPINBOX, CALLBACK) \
	connect(m_ui->SPINBOX, static_cast<void(QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this]() -> void CALLBACK)
#define cSlider(SLIDER, CALLBACK) \
	connect(m_ui->SLIDER, &QSlider::valueChanged, [this]() -> void CALLBACK)

	// UI signals/slots
	cAction(actionPageGeneral,     { m_ui->realCentralWidget->setCurrentIndex(0); });
	cAction(actionPageGraphics,    { m_ui->realCentralWidget->setCurrentIndex(1); });
	cAction(actionPageAudio,       { m_ui->realCentralWidget->setCurrentIndex(2); });
	cAction(actionPageControllers, { m_ui->realCentralWidget->setCurrentIndex(3); });

	/* Settings signals/slots */
	// General - Basic
	cCheck(chkCheats,   { SCGI.bEnableCheats = m_ui->chkCheats->isChecked(); });
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
	cSpin(sbFramelimit, {
		int valmod5 = m_ui->sbFramelimit->value() % 5;
		if (valmod5 != 0)
			m_ui->sbFramelimit->setValue(m_ui->sbFramelimit->value() - valmod5);
		SCGI.m_Framelimit = (m_ui->sbFramelimit->value() / 5) + 1;
	});
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
	// General - GameCube
	// General - Wii
	// General - Advanced
	cCheck(chkForceNTSCJ, { SCGI.bForceNTSCJ = m_ui->chkForceNTSCJ->isChecked(); });
	cCheck(chkDualcore,   { SCGI.bCPUThread = m_ui->chkDualcore->isChecked(); });
	cCheck(chkIdleSkip,   { SCGI.bSkipIdle = m_ui->chkIdleSkip->isChecked(); });
	cCombo(cmbCpuEngine,  { SCGI.iCPUCore = s_cpu_engines.key(m_ui->cmbCpuEngine->currentText()); });
	cGbCheck(gbCpuOverclock, {
		SCGI.m_OCEnable = m_ui->gbCpuOverclock->isChecked();
		UpdateCpuOCLabel();
	});
	cSlider(slCpuOCFactor, {
		SCGI.m_OCFactor = std::exp2f((m_ui->slCpuOCFactor->value() - 100.f) / 25.f);
		UpdateCpuOCLabel();
	});
}

void DConfigDialog::LoadSettings()
{
	const SConfig& sconf = SConfig::GetInstance();

	// General - Basic
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
	// General - GameCube
	// General - Wii
	// General - Advanced
	m_ui->chkForceNTSCJ->setChecked(sconf.bForceNTSCJ);
	m_ui->chkDualcore->setChecked(sconf.bCPUThread);
	m_ui->chkIdleSkip->setChecked(sconf.bSkipIdle);
	m_ui->cmbCpuEngine->setCurrentText(s_cpu_engines.value(sconf.iCPUCore));
	m_ui->gbCpuOverclock->setChecked(sconf.m_OCEnable);
	m_ui->slCpuOCFactor->setValue((int)(std::log2f(sconf.m_OCFactor) * 25.f + 100.f + 0.5f));
	UpdateCpuOCLabel();
}

void DConfigDialog::UpdateCpuOCLabel()
{
	bool wii = SCGI.bWii;
	int percent = (int)(std::roundf(SCGI.m_OCFactor * 100.f));
	int clock = (int)(std::roundf(SCGI.m_OCFactor * (wii ? 729.f : 486.f)));
	m_ui->lblCpuOCFactor->setText(QStringLiteral("%1% (%2 MHz)").arg(percent).arg(clock));
}
