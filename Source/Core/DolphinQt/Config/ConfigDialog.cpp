// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <memory>
#include <QActionGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSysInfo>

#include "ui_ConfigDialog.h"

#include "AudioCommon/AudioCommon.h"
#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"
#include "Core/PowerPC/PowerPC.h"

#include "DolphinQt/MainWindow.h"
#include "DolphinQt/Config/ConfigDialog.h"
#include "DolphinQt/Utils/Resources.h"

#include "UICommon/UICommon.h"

DConfigDialog::DConfigDialog(QWidget* parent_widget)
	: QMainWindow(parent_widget),
	m_conf(SConfig::GetInstance())
{
	setWindowModality(Qt::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	// Initial UI setup
	m_ui = std::make_unique<Ui::DConfigDialog>();
	m_ui->setupUi(this);
	UpdateIcons();
#if defined(_WIN32)
	m_ui->cmbGfxBackend->setToolTip(tr("Selects what graphics API to use internally.\nThe software renderer "
		"is extremely slow and only useful for debugging, so you'll want to use either Direct3D or OpenGL. "
		"Different games and different GPUs will behave differently on each backend, so for the best "
		"emulation experience it's recommended to try both and choose the one that's less problematic.\n\n"
		"If unsure, select OpenGL."));
#else
	m_ui->cmbGfxBackend->setToolTip(tr("Selects what graphics API to use internally.\nThe software renderer "
		"is extremely slow and only useful for debugging, so unless you have a reason to use it you'll want "
		"to select OpenGL here.\n\nIf unsure, select OpenGL."));
#endif

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

	// Populate controls (that aren't translation-specific) & etc.
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
	m_ui->fcDefaultROM->setFormat(false, tr("All supported ROMs (%1);;All files (*)")
		.arg(QStringLiteral("*.gcm *.iso *.ciso *.gcz *.wbfs *.elf *.dol *.dff *.tmd *.wad")));
	m_ui->fcDVDRoot->setFormat(true, QStringLiteral(""));
	m_ui->fcApploader->setFormat(false, QStringLiteral("Apploader (*.img)"));
	m_ui->fcWiiNandRoot->setFormat(true, QStringLiteral(""));
	for (const std::string& backend : AudioCommon::GetSoundBackends())
		m_ui->cmbAudioBackend->insertItem(m_ui->cmbAudioBackend->count(), QString::fromStdString(backend));

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

void DConfigDialog::eventFilter(QObject* watched, QEvent* event)
{
	// TODO
}

// Static data (translation-specific mappings to enums, etc.)
static QMap<int, QString> s_cpu_engines;
static QMap<TEXIDevices, QString> s_exi_devices;

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
	m_ui->cmbCpuEngine->insertItems(0, s_cpu_engines.values());

	s_exi_devices = {
		{ EXIDEVICE_NONE, tr("<Nothing>") },
		{ EXIDEVICE_DUMMY, tr("Dummy") },

		{ EXIDEVICE_MEMORYCARD, tr("Memory Card") },
		{ EXIDEVICE_MEMORYCARDFOLDER, tr("GCI Folder") },
		{ EXIDEVICE_MIC, tr("Mic") },
		{ EXIDEVICE_ETH, QStringLiteral("BBA") },
		{ EXIDEVICE_AGP, QStringLiteral("Advance Game Port") },
		{ EXIDEVICE_AM_BASEBOARD, tr("AM-Baseboard") },
		{ EXIDEVICE_GECKO, QStringLiteral("USBGecko") }
	};
	m_ui->cmbGCSlotA->clear();
	m_ui->cmbGCSlotA->insertItems(0, s_exi_devices.values());
	m_ui->cmbGCSlotB->clear();
	m_ui->cmbGCSlotB->insertItems(0, s_exi_devices.values());
	m_ui->cmbGCSP1->clear();
	m_ui->cmbGCSP1->insertItems(0, { tr("<Nothing>"), tr("Dummy"),
		QStringLiteral("BBA"), tr("AM-Baseboard") });
}

void DConfigDialog::SetupSlots()
{
	// Helper macros for signal/slot creation
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
	cCheck(chkCheats,   { m_conf.bEnableCheats = m_ui->chkCheats->isChecked(); });
	cCombo(cmbFramelimit, {
		unsigned int framelimit = m_ui->cmbFramelimit->currentIndex();
		if (framelimit == 2)
		{
			m_ui->sbFramelimit->setEnabled(true);
			framelimit = (m_ui->sbFramelimit->value() / 5) + 1;
		}
		else
			m_ui->sbFramelimit->setEnabled(false);
		m_conf.m_Framelimit = framelimit;
		});
	cSpin(sbFramelimit, {
		int valmod5 = m_ui->sbFramelimit->value() % 5;
		if (valmod5 != 0)
			m_ui->sbFramelimit->setValue(m_ui->sbFramelimit->value() - valmod5);
		m_conf.m_Framelimit = (m_ui->sbFramelimit->value() / 5) + 1;
	});
	// General - Interface
	cCheck(chkConfirmStop,    { m_conf.bConfirmStop = m_ui->chkConfirmStop->isChecked(); });
	cCheck(chkPanicHandlers,  { m_conf.bUsePanicHandlers = m_ui->chkPanicHandlers->isChecked(); });
	cCheck(chkOSDMessages,    { m_conf.bOnScreenDisplayMessages = m_ui->chkOSDMessages->isChecked(); });
	cCheck(chkPauseFocusLost, { m_conf.m_PauseOnFocusLost = m_ui->chkPauseFocusLost->isChecked(); });
	cCombo(cmbTheme, {
		m_conf.theme_name = m_ui->cmbTheme->currentText().toStdString();
		Resources::Init();
		g_main_window->UpdateIcons();
		UpdateIcons();
	});
	// General - GameCube
	cCheck(chkGCSkipBios, { m_conf.bHLE_BS2 = m_ui->chkGCSkipBios->isChecked(); });
	cCheck(chkGCOverrideLang, { m_conf.bOverrideGCLanguage = m_ui->chkGCOverrideLang->isChecked(); });
	cCombo(cmbGCIplLang, { m_conf.SelectedLanguage = m_ui->cmbGCIplLang->currentIndex(); });
	cCombo(cmbGCSlotA, {
		int device = m_conf.m_EXIDevice[0] = s_exi_devices.key(m_ui->cmbGCSlotA->currentText());
		if (device != EXIDEVICE_MEMORYCARD && device != EXIDEVICE_AGP)
			m_ui->btnGCSlotA->setEnabled(false);
		else
			m_ui->btnGCSlotA->setEnabled(true);
	});
	connect(m_ui->btnGCSlotA, &QToolButton::pressed, [this]() -> void { ChooseSlotPath(0); });
	cCombo(cmbGCSlotB, {
		int device = m_conf.m_EXIDevice[1] = s_exi_devices.key(m_ui->cmbGCSlotB->currentText());
		if (device != EXIDEVICE_MEMORYCARD && device != EXIDEVICE_AGP)
			m_ui->btnGCSlotB->setEnabled(false);
		else
			m_ui->btnGCSlotB->setEnabled(true);
	});
	connect(m_ui->btnGCSlotB, &QToolButton::pressed, [this]() -> void { ChooseSlotPath(1); });
	cCombo(cmbGCSP1, { m_conf.m_EXIDevice[2] = s_exi_devices.key(m_ui->cmbGCSP1->currentText()); });
	// General - Wii
	cCheck(chkWiiScreensaver, { m_conf.m_SYSCONF->SetData("IPL.SSV", m_ui->chkWiiScreensaver->isChecked()); });
	cCheck(chkWiiPAL60, {
		m_conf.bPAL60 = m_ui->chkWiiPAL60->isChecked();
		m_conf.m_SYSCONF->SetData("IPL.E60", m_ui->chkWiiPAL60->isChecked());
	});
	cCombo(cmbWiiAR, { m_conf.m_SYSCONF->SetData("IPL.AR", m_ui->cmbWiiAR->currentIndex()); });
	cCombo(cmbWiiLang, {
		DiscIO::IVolume::ELanguage wii_system_lang = (DiscIO::IVolume::ELanguage)m_ui->cmbWiiLang->currentIndex();
		m_conf.m_SYSCONF->SetData("IPL.LNG", wii_system_lang);
		u8 country_code = UICommon::GetSADRCountryCode(wii_system_lang);
		if (!m_conf.m_SYSCONF->SetArrayData("IPL.SADR", &country_code, 1))
			QMessageBox::critical(this, tr("Error"), tr("Failed to update country code in SYSCONF"),
				QMessageBox::Ok, QMessageBox::NoButton);
	});
	cCheck(chkWiiSDCard, {
		m_conf.m_WiiSDCard = m_ui->chkWiiSDCard->isChecked();
		WII_IPC_HLE_Interface::SDIO_EventNotify();
	});
	cCheck(chkWiiUSBKeyboard, { m_conf.m_WiiKeyboard = m_ui->chkWiiUSBKeyboard->isChecked(); });
	// General - Paths
	cCheck(chkSearchSubfolders, { m_conf.m_RecursiveISOFolder = m_ui->chkSearchSubfolders->isChecked(); });
	connect(m_ui->listDirectories, &QListWidget::currentRowChanged, [this](int row) -> void {
		if (row == -1)
			m_ui->btnRemoveDirectory->setEnabled(false);
		else
			m_ui->btnRemoveDirectory->setEnabled(true);
	});
	connect(m_ui->btnAddDirectory, &QPushButton::pressed, [this]() -> void {
		QString path = QFileDialog::getExistingDirectory(this, tr("Select directory"),
			QStandardPaths::writableLocation(QStandardPaths::HomeLocation), QFileDialog::ShowDirsOnly);
#ifdef Q_OS_WIN
		path.replace(QStringLiteral("/"), QStringLiteral("\\"));
#endif
		m_ui->listDirectories->insertItem(m_ui->listDirectories->count(), path);
		m_conf.m_ISOFolder.push_back(path.toStdString());
	});
	connect(m_ui->btnRemoveDirectory, &QPushButton::pressed, [this]() -> void {
		QListWidgetItem* i = m_ui->listDirectories->takeItem(m_ui->listDirectories->currentRow());
		m_conf.m_ISOFolder.erase(std::remove(m_conf.m_ISOFolder.begin(), m_conf.m_ISOFolder.end(),
			i->text().toStdString()), m_conf.m_ISOFolder.end());
		delete i;
	});
	connect(m_ui->fcDefaultROM, &DFileChooser::changed, [this]() -> void {
		m_conf.m_strDefaultISO = m_ui->fcDefaultROM->path().toStdString(); });
	connect(m_ui->fcDVDRoot, &DFileChooser::changed, [this]() -> void {
		m_conf.m_strDVDRoot = m_ui->fcDVDRoot->path().toStdString(); });
	connect(m_ui->fcApploader, &DFileChooser::changed, [this]() -> void {
		m_conf.m_strApploader = m_ui->fcApploader->path().toStdString(); });
	connect(m_ui->fcWiiNandRoot, &DFileChooser::changed, [this]() -> void {
		m_conf.m_NANDPath = m_ui->fcWiiNandRoot->path().toStdString(); });
	// General - Advanced
	cCheck(chkForceNTSCJ, { m_conf.bForceNTSCJ = m_ui->chkForceNTSCJ->isChecked(); });
	cCheck(chkDualcore,   { m_conf.bCPUThread = m_ui->chkDualcore->isChecked(); });
	cCheck(chkIdleSkip,   { m_conf.bSkipIdle = m_ui->chkIdleSkip->isChecked(); });
	cCombo(cmbCpuEngine,  { m_conf.iCPUCore = s_cpu_engines.key(m_ui->cmbCpuEngine->currentText()); });
	cGbCheck(gbCpuOverclock, {
		m_conf.m_OCEnable = m_ui->gbCpuOverclock->isChecked();
		UpdateCpuOCLabel();
	});
	cSlider(slCpuOCFactor, {
		m_conf.m_OCFactor = std::exp2f((m_ui->slCpuOCFactor->value() - 100.f) / 25.f);
		UpdateCpuOCLabel();
	});
	// Graphics - General
	// Audio
	cCombo(cmbAudioBackend, { AudioBackendChanged(); });
	cSpin(sbLatency, { m_conf.iLatency = m_ui->sbLatency->value(); });
	cCheck(chkDPL2, { m_conf.bDPL2Decoder = m_ui->chkDPL2->isChecked(); });
	cCombo(cmbDSPEngine, {
		m_conf.bDSPHLE = m_ui->cmbDSPEngine->currentIndex() == 0;
		m_conf.m_DSPEnableJIT = m_ui->cmbDSPEngine->currentIndex() == 1;
		AudioCommon::UpdateSoundStream();
	});
	cSlider(slVolume, {
		m_ui->lblVolume->setText(QStringLiteral("%1%").arg(m_ui->slVolume->value()));
		m_conf.m_Volume = m_ui->slVolume->value();
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
	if (!File::Exists(File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + USA_DIR + DIR_SEP GC_IPL) &&
		!File::Exists(File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + USA_DIR + DIR_SEP GC_IPL) &&
		!File::Exists(File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + JAP_DIR + DIR_SEP GC_IPL) &&
		!File::Exists(File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + JAP_DIR + DIR_SEP GC_IPL) &&
		!File::Exists(File::GetUserPath(D_GCUSER_IDX) + DIR_SEP + EUR_DIR + DIR_SEP GC_IPL) &&
		!File::Exists(File::GetSysDirectory() + GC_SYS_DIR + DIR_SEP + EUR_DIR + DIR_SEP GC_IPL))
	{
		m_ui->chkGCSkipBios->setEnabled(false);
		m_ui->chkGCSkipBios->setToolTip(tr("Put BIOS roms in User/GC/{region}."));
	}
	m_ui->chkGCSkipBios->setChecked(sconf.bHLE_BS2);
	m_ui->cmbGCIplLang->setCurrentIndex(sconf.SelectedLanguage);
	m_ui->chkGCOverrideLang->setChecked(sconf.bOverrideGCLanguage);
	m_ui->cmbGCSlotA->setCurrentText(s_exi_devices.value(sconf.m_EXIDevice[0]));
	m_ui->cmbGCSlotB->setCurrentText(s_exi_devices.value(sconf.m_EXIDevice[1]));
	m_ui->cmbGCSP1->setCurrentText(s_exi_devices.value(sconf.m_EXIDevice[2]));
	// General - Wii
	m_ui->chkWiiScreensaver->setChecked(!!sconf.m_SYSCONF->GetData<u8>("IPL.SSV"));
	m_ui->chkWiiPAL60->setChecked(sconf.bPAL60);
	m_ui->cmbWiiAR->setCurrentIndex(sconf.m_SYSCONF->GetData<u8>("IPL.AR"));
	m_ui->cmbWiiLang->setCurrentIndex(sconf.m_SYSCONF->GetData<u8>("IPL.LNG"));
	m_ui->chkWiiSDCard->setChecked(sconf.m_WiiSDCard);
	m_ui->chkWiiUSBKeyboard->setChecked(sconf.m_WiiKeyboard);
	// General - Paths
	for (const std::string& folder : sconf.m_ISOFolder)
		m_ui->listDirectories->insertItem(m_ui->listDirectories->count(),
			QString::fromStdString(folder));
	m_ui->chkSearchSubfolders->setChecked(sconf.m_RecursiveISOFolder);
	m_ui->fcDefaultROM->setPath(QString::fromStdString(sconf.m_strDefaultISO));
	m_ui->fcDVDRoot->setPath(QString::fromStdString(sconf.m_strDVDRoot));
	m_ui->fcApploader->setPath(QString::fromStdString(sconf.m_strApploader));
	m_ui->fcWiiNandRoot->setPath(QString::fromStdString(sconf.m_NANDPath));
	// General - Advanced
	m_ui->chkForceNTSCJ->setChecked(sconf.bForceNTSCJ);
	m_ui->chkDualcore->setChecked(sconf.bCPUThread);
	m_ui->chkIdleSkip->setChecked(sconf.bSkipIdle);
	m_ui->cmbCpuEngine->setCurrentText(s_cpu_engines.value(sconf.iCPUCore));
	m_ui->gbCpuOverclock->setChecked(sconf.m_OCEnable);
	m_ui->slCpuOCFactor->setValue((int)(std::log2f(sconf.m_OCFactor) * 25.f + 100.f + 0.5f));
	UpdateCpuOCLabel();
	// Audio
	m_ui->cmbAudioBackend->setCurrentText(QString::fromStdString(sconf.sBackend));
	AudioBackendChanged();
	m_ui->sbLatency->setValue(sconf.iLatency);
	m_ui->chkDPL2->setChecked(sconf.bDPL2Decoder);
	if (sconf.bDSPHLE)
		m_ui->cmbDSPEngine->setCurrentIndex(0);
	else
		m_ui->cmbDSPEngine->setCurrentIndex(sconf.m_DSPEnableJIT ? 1 : 2);
	m_ui->slVolume->setValue(m_conf.m_Volume);
}

void DConfigDialog::UpdateCpuOCLabel()
{
	int percent = (int)(std::roundf(m_conf.m_OCFactor * 100.f));
	int clock = (int)(std::roundf(m_conf.m_OCFactor * (m_conf.bWii ? 729.f : 486.f)));
	m_ui->lblCpuOCFactor->setText(QStringLiteral("%1% (%2 MHz)").arg(percent).arg(clock));
}

void DConfigDialog::ChooseSlotPath(int device)
{
	const std::string& oldPath = (device == 0) ? m_conf.m_strMemoryCardA : m_conf.m_strMemoryCardB;
	QString filter = (m_conf.m_EXIDevice[device] == EXIDEVICE_MEMORYCARD) ?
		tr("GameCube Memory Cards (%1)").arg(QStringLiteral("*.raw *.gcp")) :
		tr("Game Boy Advance Carts (%1)").arg(QStringLiteral("*.gba"));
	QString path = QFileDialog::getOpenFileName(this, tr("Choose file"),
		QFileInfo(QString::fromStdString(oldPath)).absoluteDir().path(), filter);
	if (!path.isEmpty())
	{
		if (device == 0)
			m_conf.m_strMemoryCardA = path.toStdString();
		else if (device == 1)
			m_conf.m_strMemoryCardB = path.toStdString();
	}
}

void DConfigDialog::AudioBackendChanged()
{
	std::string backend = m_ui->cmbAudioBackend->currentText().toStdString();
	m_ui->gbVolume->setEnabled(backend == BACKEND_COREAUDIO ||
		backend == BACKEND_OPENAL || backend == BACKEND_XAUDIO2);
	m_ui->sbLatency->setEnabled(backend == BACKEND_OPENAL);
	m_ui->chkDPL2->setEnabled(backend == BACKEND_OPENAL || backend == BACKEND_PULSEAUDIO);
	m_conf.sBackend = backend;
	AudioCommon::UpdateSoundStream();
}
