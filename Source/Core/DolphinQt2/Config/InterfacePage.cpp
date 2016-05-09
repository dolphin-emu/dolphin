#include "Core/ConfigManager.h"

#include "DolphinQt2/Config/InterfacePage.h"

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

void InterfacePage::LoadConfig()
{
    const SConfig& startup_params = SConfig::GetInstance();
}

void InterfacePage::SaveConfig()
{

}

void InterfacePage::BuildUIOptions()
{
    QGroupBox* uiGroup = new QGroupBox(tr("User Interface"));
    QVBoxLayout* uiGroup_layout = new QVBoxLayout;
    uiGroup->setLayout(uiGroup_layout);
    mainLayout->addWidget(uiGroup);

    {
        QHBoxLayout* themeLayout = new QHBoxLayout;
        QLabel* themeLabel = new QLabel(tr("Theme:"));
        QComboBox* themeSelector = new QComboBox;

        //TO-DO: Copy how DolphinWX does it
        themeSelector->addItem(tr("Clean"));
        themeLabel->setMaximumWidth(100);

        themeLayout->addWidget(themeLabel);
        themeLayout->addWidget(themeSelector);

        uiGroup_layout->addLayout(themeLayout);
    }

    QCheckBox* autoAdjustWindowSize = new QCheckBox(tr("Auto Adjust Window Size"));
    QCheckBox* keepDolphinOnTop = new QCheckBox(tr("Keep Dolphin on Top"));
    QCheckBox* renderToMainWindow = new QCheckBox(tr("Render to Main Window"));

    uiGroup_layout->addWidget(autoAdjustWindowSize);
    uiGroup_layout->addWidget(keepDolphinOnTop);
    uiGroup_layout->addWidget(renderToMainWindow);
}

void InterfacePage::BuildInGameOptions()
{
    QGroupBox* inGameGroup = new QGroupBox(tr("In Game"));
    QVBoxLayout* inGameGroup_layout = new QVBoxLayout;
    inGameGroup->setLayout(inGameGroup_layout);
    mainLayout->addWidget(inGameGroup);

    QCheckBox* confirmOnStop = new QCheckBox(tr("Confirm on Stop"));
    QCheckBox* usePanicHandlers = new QCheckBox(tr("Use Panic Handlers"));
    QCheckBox* enableOnScreenDisplay = new QCheckBox(tr("On Screen Messages"));
    QCheckBox* pauseOnFocusLost = new QCheckBox(tr("Pause on Focus Lost"));
    QCheckBox* hideMouseCursor = new QCheckBox(tr("Hide Mouse Cursor"));

    inGameGroup_layout->addWidget(confirmOnStop);
    inGameGroup_layout->addWidget(usePanicHandlers);
    inGameGroup_layout->addWidget(enableOnScreenDisplay);
    inGameGroup_layout->addWidget(pauseOnFocusLost);
    inGameGroup_layout->addWidget(hideMouseCursor);
}

void InterfacePage::BuildOptions()
{
    BuildUIOptions();
    BuildInGameOptions();
}

InterfacePage::InterfacePage()
{
    mainLayout = new QVBoxLayout;
    BuildOptions();
    LoadConfig();
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}
