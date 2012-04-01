#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QRadioButton>

#include "ConfigGeneral.h"
#include "../Util/Util.h"

#include "ConfigManager.h"


QWidget* DConfigMainGeneralTab::CreateCoreTabWidget(QWidget* parent)
{
	QWidget* tab = new QWidget(this);

	// Widgets
	chFramelimit = new QComboBox;
	chFramelimit->addItem(tr("None"));
	chFramelimit->addItem(tr("Auto"));
	chFramelimit->addItem(tr("Audio"));
	for (int i = 10; i <= 120; i += 5)      // from 10 to 120
                chFramelimit->addItem(QString::number(i));
	chFramelimit->setMaximumWidth(chFramelimit->minimumSizeHint().width());
	cbFPSLimit = new QCheckBox(tr("Limit by FPS"));

	rbCPUEngine = new QButtonGroup(tab);
	rbCPUEngine->addButton(new QRadioButton(tr("Interpreter")), 0);
	rbCPUEngine->addButton(new QRadioButton(tr("JIT Recompiler")), 1);
	rbCPUEngine->addButton(new QRadioButton(tr("JITIL Recompiler")), 2);


	// Layouts
	DGroupBoxV* coreSettingsBox = new DGroupBoxV(tr("Core Settings"));
	coreSettingsBox->addWidget(cbDualCore = new QCheckBox(tr("Enable Dual Core")));
	coreSettingsBox->addWidget(cbIdleSkipping = new QCheckBox(tr("Enable Idle Skipping")));
	coreSettingsBox->addWidget(cbCheats = new QCheckBox(tr("Enable Cheats")));

	QHBoxLayout* framelimitLayout = new QHBoxLayout;
	framelimitLayout->addWidget(new QLabel(tr("Framelimit:")));
	framelimitLayout->addWidget(chFramelimit);
	framelimitLayout->addWidget(cbFPSLimit);

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
	mainLayout->addStretch();
	tab->setLayout(mainLayout);


	// Initial values
	SCoreStartupParameter& Startup = SConfig::GetInstance().m_LocalCoreStartupParameter;
	ctrlManager = new DControlStateManager(this);

	ctrlManager->RegisterControl(cbDualCore, Startup.bCPUThread);
	ctrlManager->RegisterControl(cbIdleSkipping, Startup.bSkipIdle);
	ctrlManager->RegisterControl(cbCheats, Startup.bEnableCheats);

	ctrlManager->RegisterControl(chFramelimit, SConfig::GetInstance().m_Framelimit);
	ctrlManager->RegisterControl(cbFPSLimit, SConfig::GetInstance().b_UseFPS);

	ctrlManager->RegisterControl(reinterpret_cast<QRadioButton*>(rbCPUEngine->button(0)), (Startup.iCPUCore == 0));
	ctrlManager->RegisterControl(reinterpret_cast<QRadioButton*>(rbCPUEngine->button(1)), (Startup.iCPUCore == 1));
	ctrlManager->RegisterControl(reinterpret_cast<QRadioButton*>(rbCPUEngine->button(2)), (Startup.iCPUCore == 2));

	ctrlManager->RegisterControl(cbConfirmOnStop, Startup.bConfirmStop);
	ctrlManager->RegisterControl(cbRenderToMain, Startup.bRenderToMain);

	return tab;
}

QWidget* DConfigMainGeneralTab::CreatePathsTabWidget(QWidget* parent)
{
	QWidget* tab = new QWidget(this);

	// Widgets
	pathList = new QListWidget;

	cbRecurse = new QCheckBox(tr("Search subfolders"));

	addPath = new QPushButton(tr("Add")); // TODO: Icon
	removePath = new QPushButton(tr("Remove")); // TODO: Icon
	clearPathList = new QPushButton(tr("Clear"));


	// Signals
	connect(addPath, SIGNAL(clicked()), this, SLOT(OnAddIsoPath()));
	connect(removePath, SIGNAL(clicked()), this, SLOT(OnRemoveIsoPath()));
	connect(clearPathList, SIGNAL(clicked()), this, SLOT(OnClearIsoPathList()));

	connect(addPath, SIGNAL(clicked()), this, SLOT(OnPathListChanged()));
	connect(removePath, SIGNAL(clicked()), this, SLOT(OnPathListChanged()));
	connect(clearPathList, SIGNAL(clicked()), this, SLOT(OnPathListChanged()));

	connect(pathList, SIGNAL(itemSelectionChanged()), this, SLOT(OnSelectionChanged()));

	// Layouts
	QBoxLayout* pathListButtonLayout = new QHBoxLayout;
	pathListButtonLayout->addWidget(clearPathList);
	pathListButtonLayout->addStretch();
	pathListButtonLayout->addWidget(addPath);
	pathListButtonLayout->addWidget(removePath);

	QBoxLayout* mainLayout = new QVBoxLayout;
	mainLayout->addWidget(pathList);
	mainLayout->addWidget(cbRecurse);
	mainLayout->addLayout(pathListButtonLayout);
	tab->setLayout(mainLayout);

	// Initial values
	ctrlManager = new DControlStateManager(this);
	ctrlManager->RegisterControl(cbRecurse, SConfig::GetInstance().m_RecursiveISOFolder);

	for (std::vector<std::string>::iterator it = SConfig::GetInstance().m_ISOFolder.begin(); it != SConfig::GetInstance().m_ISOFolder.end(); ++it)
		pathList->addItem(new QListWidgetItem(QString::fromStdString(*it)));
	if (!pathList->count())
		clearPathList->setDisabled(true);
	removePath->setDisabled(true);

	return tab;
}

DConfigMainGeneralTab::DConfigMainGeneralTab(QWidget* parent) : QTabWidget(parent), paths_changed(false)
{
	addTab(CreateCoreTabWidget(this), tr("Core"));
	addTab(CreatePathsTabWidget(this), tr("Paths"));
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

	SConfig::GetInstance().m_Framelimit = chFramelimit->currentIndex();
	SConfig::GetInstance().b_UseFPS = cbFPSLimit->isChecked();

	Startup.iCPUCore = rbCPUEngine->checkedId();

	Startup.bConfirmStop = cbConfirmOnStop->isChecked();
	Startup.bRenderToMain = cbRenderToMain->isChecked();

	SConfig::GetInstance().m_RecursiveISOFolder = cbRecurse->isChecked();

	if (paths_changed)
	{
		SConfig::GetInstance().m_ISOFolder.clear();
		for (int i = 0; i < pathList->count(); ++i)
			SConfig::GetInstance().m_ISOFolder.push_back(pathList->item(i)->text().toStdString());

		emit IsoPathsChanged();
		paths_changed = false;
	}
}

void DConfigMainGeneralTab::OnAddIsoPath()
{
	QString selection = QFileDialog::getExistingDirectory(this, tr("Select a directory to add"));
	if(selection.length() == 0) return;

	pathList->addItem(selection);
	clearPathList->setDisabled(false);
}

void DConfigMainGeneralTab::OnRemoveIsoPath()
{
	delete pathList->takeItem(pathList->row(pathList->currentItem()));
}

void DConfigMainGeneralTab::OnClearIsoPathList()
{
	pathList->clear();
	clearPathList->setDisabled(true);
}

void DConfigMainGeneralTab::OnPathListChanged()
{
	paths_changed = true;
}

void DConfigMainGeneralTab::OnSelectionChanged()
{
	removePath->setDisabled(!pathList->currentItem());

}
