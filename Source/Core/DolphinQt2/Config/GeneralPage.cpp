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
}

void GeneralPage::SaveConfig()
{
    //const SConfig& save_params = SConfig::GetInstance();
}

void GeneralPage::BuildBasicSettings()
{
    QGroupBox* basicGroup = new QGroupBox(tr("Basic Settings"));
    QVBoxLayout* basicGroupLayout = new QVBoxLayout;
    basicGroup->setLayout(basicGroupLayout);
    mainLayout->addWidget(basicGroup);

    language = new QComboBox;
    language->addItem(tr("English"));
    enableCheats   = new QCheckBox(tr("Enable Cheats"));

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
    setLayout(mainLayout);
}