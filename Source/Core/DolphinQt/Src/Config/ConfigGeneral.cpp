#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QRadioButton>

#include "ConfigGeneral.h"
#include "Util.h"

#include "ConfigManager.h"


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
	ctrlManager = new DControlStateManager(this);

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
