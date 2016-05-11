#include "Common/CommonPaths.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Config/Pages/InterfacePage.h"

void InterfacePage::LoadConfig()
{
    const SConfig& startup_params = SConfig::GetInstance();

    // UI Options
    autoAdjustWindowSize->setChecked(startup_params.bRenderWindowAutoSize);
    keepDolphinOnTop->setChecked(startup_params.bKeepWindowOnTop);
    renderToMainWindow->setChecked(startup_params.bRenderToMain);
    themeSelector->setCurrentIndex(themeSelector->findText(QString::fromStdString(SConfig::GetInstance().theme_name)));

    // In Game Options
    confirmOnStop->setChecked(startup_params.bConfirmStop);
    usePanicHandlers->setChecked(startup_params.bUsePanicHandlers);
    enableOnScreenDisplay->setChecked(startup_params.bOnScreenDisplayMessages);
    pauseOnFocusLost->setChecked(startup_params.m_PauseOnFocusLost);
    hideMouseCursor->setChecked(startup_params.bAutoHideCursor);

}

void InterfacePage::SaveConfig()
{

    // UI Options
    SConfig::GetInstance().bRenderWindowAutoSize = autoAdjustWindowSize->isChecked();
    SConfig::GetInstance().bKeepWindowOnTop = keepDolphinOnTop->isChecked();
    SConfig::GetInstance().bRenderToMain = renderToMainWindow->isChecked();
    SConfig::GetInstance().theme_name = themeSelector->currentText().toStdString();

    // In Game Options
    SConfig::GetInstance().bConfirmStop = confirmOnStop->isChecked();
    SConfig::GetInstance().bUsePanicHandlers = usePanicHandlers->isChecked();
    SConfig::GetInstance().bOnScreenDisplayMessages = enableOnScreenDisplay->isChecked();
    SConfig::GetInstance().m_PauseOnFocusLost = pauseOnFocusLost->isChecked();
    SConfig::GetInstance().bAutoHideCursor = hideMouseCursor->isChecked();

    SConfig::GetInstance().SaveSettings();
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
        themeSelector = new QComboBox;

        auto sv = DoFileSearch({""}, {
            File::GetUserPath(D_THEMES_IDX),
            File::GetSysDirectory() + THEMES_DIR
        }, /*recursive*/ false);

        for (const std::string& filename : sv)
        {
            std::string name, ext;
            SplitPath(filename, nullptr, &name, &ext);
            name += ext;
            QString qtname = QString::fromStdString(name);
            themeSelector->addItem(qtname);
        }

        themeLabel->setMaximumWidth(150);

        themeLayout->addWidget(themeLabel);
        themeLayout->addWidget(themeSelector);

        uiGroup_layout->addLayout(themeLayout);
    }

    autoAdjustWindowSize = new QCheckBox(tr("Auto Adjust Window Size"));
    keepDolphinOnTop = new QCheckBox(tr("Keep Dolphin on Top"));
    renderToMainWindow = new QCheckBox(tr("Render to Main Window"));

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

    confirmOnStop = new QCheckBox(tr("Confirm on Stop"));
    usePanicHandlers = new QCheckBox(tr("Use Panic Handlers"));
    enableOnScreenDisplay = new QCheckBox(tr("On Screen Messages"));
    pauseOnFocusLost = new QCheckBox(tr("Pause on Focus Lost"));
    hideMouseCursor = new QCheckBox(tr("Hide Mouse Cursor"));

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

void InterfacePage::ConnectOptions()
{
    // UI Options
    connect(autoAdjustWindowSize, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
    connect(keepDolphinOnTop, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
    connect(renderToMainWindow, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
    connect(themeSelector, static_cast<void(QComboBox::*)(const QString &)>(&QComboBox::activated),
    [=](const QString &text){ SaveConfig(); });
    // In Game
    connect(confirmOnStop, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
    connect(usePanicHandlers, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
    connect(enableOnScreenDisplay, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
    connect(pauseOnFocusLost, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
    connect(hideMouseCursor, &QCheckBox::clicked, this, &InterfacePage::SaveConfig);
}

InterfacePage::InterfacePage(QWidget* parent)
{
    mainLayout = new QVBoxLayout;
    BuildOptions();
    ConnectOptions();
    LoadConfig();
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}
