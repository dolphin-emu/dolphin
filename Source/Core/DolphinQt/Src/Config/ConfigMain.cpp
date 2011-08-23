#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QRadioButton>
#include <QStackedWidget>

#include "ConfigMain.h"
#include "ConfigAudio.h"
#include "ConfigGfx.h"
#include "ConfigPad.h"
#include "ConfigWiimote.h"
#include "Util.h"
#include "Resources.h"

#include "ConfigManager.h"

DConfigControlManager::DConfigControlManager(QObject* parent) : QObject(parent)
{
}

void DConfigControlManager::RegisterControl(QCheckBox* control, bool checked)
{
	checkbox_states.insert(control, checked);
	control->setChecked(checked);
}

void DConfigControlManager::RegisterControl(QRadioButton* control, bool checked)
{
	radiobutton_states.insert(control, checked);
	control->setChecked(checked);
}

void DConfigControlManager::RegisterControl(QComboBox* control, int choice)
{
	combobox_states_int.insert(control, choice);
	control->setCurrentIndex(choice);
}

void DConfigControlManager::RegisterControl(QComboBox* control, const QString& choice)
{
	combobox_states_string.insert(control, choice);
	control->setCurrentIndex(control->findText(choice)); // TODO: Make sure the text is valid
}


void DConfigControlManager::OnReset()
{
	for (QMap<QCheckBox*,bool>::iterator cb = checkbox_states.begin(); cb != checkbox_states.end(); ++cb)
		cb.key()->setChecked(cb.value());

	for (QMap<QRadioButton*,bool>::iterator rb = radiobutton_states.begin(); rb != radiobutton_states.end(); ++rb)
		rb.key()->setChecked(rb.value());

	for (QMap<QComboBox*,int>::iterator cb = combobox_states_int.begin(); cb != combobox_states_int.end(); ++cb)
		cb.key()->setCurrentIndex(cb.value());

	for (QMap<QComboBox*,QString>::iterator cb = combobox_states_string.begin(); cb != combobox_states_string.end(); ++cb)
		cb.key()->setCurrentIndex(cb.key()->findText(cb.value())); // TODO: Make sure the text is valid
}


DConfigMainGeneralTab::DConfigMainGeneralTab(QWidget* parent): QWidget(parent)
{
	// Widgets
	chFramelimit = new QComboBox;
	chFramelimit->addItem(tr("Rendered Frames"));
	chFramelimit->addItem(tr("Video Interrupts"));
	chFramelimit->addItem(tr("Audio"));

	rbCPUEngine = new QButtonGroup(this);
	rbCPUEngine->addButton(new QRadioButton(tr("Interpreter")), 0);
	rbCPUEngine->addButton(new QRadioButton(tr("JIT Recompiler")), 1);
	rbCPUEngine->addButton(new QRadioButton(tr("JITIL Recompiler")), 2);


	// Layouts
	DGroupBoxV* coreSettingsBox = new DGroupBoxV(tr("Core Settings"));
	coreSettingsBox->addWidget(cbDualCore = new QCheckBox(tr("Enable Dual Core")));
	coreSettingsBox->addWidget(cbIdleSkipping = new QCheckBox(tr("Enable Idle Skipping")));
	coreSettingsBox->addWidget(cbCheats = new QCheckBox(tr("Enable Cheats")));
	coreSettingsBox->addWidget(chFramelimit);

	QHBoxLayout* framelimitLayout = new QHBoxLayout;
	framelimitLayout->addWidget(new QLabel(tr("Limit by:")));
	framelimitLayout->addWidget(chFramelimit);
	// TODO: Add frame count combo box, add a custom event to hide it eventually
	coreSettingsBox->addLayout(framelimitLayout);

	DGroupBoxV* CPUEngineBox = new DGroupBoxV(tr("CPU Engine"));
	CPUEngineBox->addWidget(rbCPUEngine->button(0));
	CPUEngineBox->addWidget(rbCPUEngine->button(1));
	CPUEngineBox->addWidget(rbCPUEngine->button(2));

	DGroupBoxV* interfaceBox = new DGroupBoxV(tr("Interface"));
	interfaceBox->addWidget(cbConfirmOnStop = new QCheckBox(tr("Confirm on Stop")));
	interfaceBox->addWidget(cbRenderToMain = new QCheckBox(tr("Render to Main Window")));

	QBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->addWidget(coreSettingsBox);
	mainLayout->addWidget(CPUEngineBox);
	mainLayout->addWidget(interfaceBox);
	setLayout(mainLayout);


	// Initial values
	SCoreStartupParameter& Startup = SConfig::GetInstance().m_LocalCoreStartupParameter;
	ctrlManager = new DConfigControlManager(this);

	ctrlManager->RegisterControl(cbDualCore, Startup.bCPUThread);
	ctrlManager->RegisterControl(cbIdleSkipping, Startup.bSkipIdle);
	ctrlManager->RegisterControl(cbCheats, Startup.bEnableCheats);

	ctrlManager->RegisterControl(chFramelimit, 0); // TODO: Correct value..

	ctrlManager->RegisterControl(reinterpret_cast<QRadioButton*>(rbCPUEngine->button(0)), (Startup.iCPUCore == 0));
	ctrlManager->RegisterControl(reinterpret_cast<QRadioButton*>(rbCPUEngine->button(1)), (Startup.iCPUCore == 1));
	ctrlManager->RegisterControl(reinterpret_cast<QRadioButton*>(rbCPUEngine->button(2)), (Startup.iCPUCore == 2));

	ctrlManager->RegisterControl(cbConfirmOnStop, Startup.bConfirmStop);
	ctrlManager->RegisterControl(cbRenderToMain, Startup.bRenderToMain);
}

void DConfigMainGeneralTab::Reset()
{
	ctrlManager->OnReset();
}

void DConfigMainGeneralTab::Apply()
{
	SCoreStartupParameter& Startup = SConfig::GetInstance().m_LocalCoreStartupParameter;
	Startup.bCPUThread = cbDualCore->isChecked();
	Startup.bSkipIdle = cbIdleSkipping->isChecked();
	Startup.bEnableCheats = cbCheats->isChecked();
	// TODO: Apply framelimit

	Startup.iCPUCore = rbCPUEngine->checkedId();

	Startup.bConfirmStop = cbConfirmOnStop->isChecked();
	Startup.bRenderToMain = cbRenderToMain->isChecked();
}

DConfigDialog::DConfigDialog(InitialConfigItem initialConfigItem, QWidget* parent) : QDialog(parent)
{
	setModal(true);
	setWindowTitle(tr("Dolphin Configuration"));

	// List view
	QListWidgetItem* generalConfigButton = new QListWidgetItem;
	generalConfigButton->setIcon(Resources::GetIcon(Resources::TOOLBAR_CONFIGURE));
	generalConfigButton->setText(tr("General"));

	QListWidgetItem* graphicsConfigButton = new QListWidgetItem;
	graphicsConfigButton->setIcon(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_GFX));
	graphicsConfigButton->setText(tr("Graphics"));

	QListWidgetItem* soundConfigButton = new QListWidgetItem;
	soundConfigButton->setIcon(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_DSP));
	soundConfigButton->setText(tr("Audio"));

	QListWidgetItem* padConfigButton = new QListWidgetItem;
	padConfigButton->setIcon(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_GCPAD));
	padConfigButton->setText(tr("GC Pad"));

	QListWidgetItem* wiimoteConfigButton = new QListWidgetItem;
	wiimoteConfigButton->setIcon(Resources::GetIcon(Resources::TOOLBAR_PLUGIN_WIIMOTE));
	wiimoteConfigButton->setText(tr("Wiimote"));

	menusView = new QListWidget;
	menusView->setViewMode(QListView::IconMode);
	menusView->setIconSize(QSize(48,48)); // TODO: Need better/bigger icons
	menusView->setMovement(QListView::Static);
	menusView->setMaximumWidth(72); // TODO: Rather, set this to the widest widget width
	menusView->setMinimumHeight(76*5); // TODO: Don't hardcode this
	menusView->setSpacing(6);

	menusView->addItem(generalConfigButton);
	menusView->addItem(graphicsConfigButton);
	menusView->addItem(soundConfigButton);
	menusView->addItem(padConfigButton);
	menusView->addItem(wiimoteConfigButton);
	menusView->setCurrentRow(initialConfigItem);


	// Configuration widgets
	DConfigMainGeneralTab* generalWidget = new DConfigMainGeneralTab;
	DConfigGfx* gfxWidget = new DConfigGfx;
	DConfigAudio* audioWidget = new DConfigAudio;
	DConfigPadWidget* padWidget = new DConfigPadWidget;
	DConfigWiimote* wiimoteWidget = new DConfigWiimote;

	connect(this, SIGNAL(Apply()), generalWidget, SLOT(Apply()));
	connect(this, SIGNAL(Apply()), gfxWidget, SLOT(Apply()));
	connect(this, SIGNAL(Apply()), audioWidget, SLOT(Apply()));
	connect(this, SIGNAL(Apply()), padWidget, SLOT(Apply()));
	connect(this, SIGNAL(Apply()), wiimoteWidget, SLOT(Apply()));
	connect(this, SIGNAL(Reset()), generalWidget, SLOT(Reset()));
	connect(this, SIGNAL(Reset()), gfxWidget, SLOT(Reset()));
	connect(this, SIGNAL(Reset()), audioWidget, SLOT(Reset()));
	connect(this, SIGNAL(Reset()), padWidget, SLOT(Reset()));
	connect(this, SIGNAL(Reset()), wiimoteWidget, SLOT(Reset()));

	stackWidget = new QStackedWidget;
	stackWidget->addWidget(generalWidget);
	stackWidget->addWidget(gfxWidget);
	stackWidget->addWidget(audioWidget);
	stackWidget->addWidget(padWidget);
	stackWidget->addWidget(wiimoteWidget);
	stackWidget->setCurrentIndex(initialConfigItem);

	connect(menusView, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
			this, SLOT(switchPage(QListWidgetItem*,QListWidgetItem*)));


	// Layout
	QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply | QDialogButtonBox::Reset);
	connect(buttons->button(QDialogButtonBox::Ok), SIGNAL(clicked()), this, SLOT(OnOk()));
	connect(buttons->button(QDialogButtonBox::Cancel), SIGNAL(clicked()), this, SLOT(close()));
	connect(buttons->button(QDialogButtonBox::Apply), SIGNAL(clicked()), this, SLOT(OnApply()));
	connect(buttons->button(QDialogButtonBox::Reset), SIGNAL(clicked()), this, SLOT(OnReset()));
	// TODO: Gray out Reset and Apply buttons until a setting was changed

	QBoxLayout* mainLayout = new QVBoxLayout;
	QBoxLayout* topLayout = new QHBoxLayout;
	topLayout->addWidget(menusView);
	topLayout->addWidget(stackWidget);
	mainLayout->addLayout(topLayout);
	mainLayout->addWidget(buttons);
	mainLayout->setSizeConstraint(QLayout::SetFixedSize);
	setLayout(mainLayout);
}

void DConfigDialog::switchPage(QListWidgetItem* cur, QListWidgetItem* prev)
{
	if (!cur) cur = prev;

	stackWidget->setCurrentIndex(menusView->row(cur));
}

void DConfigDialog::OnOk()
{
	emit Apply();
	close();
}

void DConfigDialog::OnApply()
{
	emit Apply();
}

void DConfigDialog::OnReset()
{
	emit Reset();
}

void DConfigDialog::closeEvent(QCloseEvent* ev)
{
	// TODO: Ask the user if he wants to keep any changes
    QDialog::closeEvent(ev);
}
