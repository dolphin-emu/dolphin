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
#include "ConfigGeneral.h"
#include "ConfigGfx.h"
#include "ConfigPad.h"
#include "ConfigWiimote.h"
#include "Util.h"
#include "Resources.h"


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
