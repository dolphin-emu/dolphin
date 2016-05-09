#include "Core/ConfigManager.h"
#include "DolphinQt2/Config/GeneralPage.h"

class QAction;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDir;
class QFileDialog;
class QFont;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QRadioButton;
class QSize;
class QSizePolicy;
class QSlider;
class QStackedWidget;
class QVBoxLayout;

SettingPage::SettingPage(QWidget* parent)
    : QWidget(parent)
{

}

void GeneralPage::LoadConfig()
{
    const SConfig& startup_params = SConfig::GetInstance();

    enableCheats->setChecked(startup_params.bEnableCheats);
    forceNTSC->setChecked(startup_params.bForceNTSCJ);

    u32 selection = qRound(startup_params.m_EmulationSpeed * 10.0f);
    if (selection < (u32)speedLimit->count())
        speedLimit->setCurrentIndex(selection);
}

void GeneralPage::SaveConfig()
{
    SConfig::GetInstance().bEnableCheats = enableCheats->isChecked();
    SConfig::GetInstance().bForceNTSCJ = forceNTSC->isChecked();
    SConfig::GetInstance().m_EmulationSpeed = speedLimit->currentIndex() * 0.1f;
}

void GeneralPage::BuildBasicSettings()
{
    QGroupBox* basicGroup = new QGroupBox(tr("Basic Settings"));
    QVBoxLayout* basicGroupLayout = new QVBoxLayout;
    basicGroup->setLayout(basicGroupLayout);
    mainLayout->addWidget(basicGroup);

    //TODO: Figure out how to populate this box
    language = new QComboBox;
    language->addItem(tr("English"));
    enableCheats = new QCheckBox(tr("Enable Cheats"));

    connect(enableCheats, &QCheckBox::clicked, this, &GeneralPage::SaveConfig);
    connect(language, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
        [=](int index){GeneralPage::SaveConfig();});

    basicGroupLayout->addWidget(language);
    basicGroupLayout->addWidget(enableCheats);
}

void GeneralPage::BuildAdvancedSettings()
{
    QGroupBox* advancedGroup = new QGroupBox(tr("Advanced Settings"));
    QVBoxLayout* advancedGroupLayout = new QVBoxLayout;
    advancedGroup->setLayout(advancedGroupLayout);
    mainLayout->addWidget(advancedGroup);

    speedLimit = new QComboBox;
    forceNTSC = new QCheckBox(tr("Force Console as NTSC-J"));

    speedLimit->addItem(tr("Unlimited"));
    for (int i = 10; i <= 200; i += 10) // from 10% to 200%
    {
        QString str;
        if (i == 100)
            str.sprintf("%i%% (Normal Speed)",i);
        else
            str.sprintf("%i%%",i);

        speedLimit->addItem(str);
    }

    connect(forceNTSC, &QCheckBox::clicked, this, &GeneralPage::SaveConfig);
    connect(speedLimit, static_cast<void(QComboBox::*)(int)>(&QComboBox::activated),
        [=](int index){GeneralPage::SaveConfig();});

    advancedGroupLayout->addWidget(speedLimit);
    advancedGroupLayout->addWidget(forceNTSC);
}

void GeneralPage::BuildOptions()
{
    BuildBasicSettings();
    BuildAdvancedSettings();
}

GeneralPage::GeneralPage()
{
    mainLayout = new QVBoxLayout;
    BuildOptions();
    LoadConfig();
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}
