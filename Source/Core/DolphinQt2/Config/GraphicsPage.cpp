#include "Core/ConfigManager.h"
#include "DolphinQt2/Config/GraphicsPage.h"

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

void GraphicsPage::LoadConfig()
{
    const SConfig& startup_params = SConfig::GetInstance();
}

void GraphicsPage::SaveConfig()
{

}

void GraphicsPage::BuildOptions()
{
    QGroupBox* basicGroup = new QGroupBox(tr("Basic Settings"));
    QVBoxLayout* basicGroupLayout = new QVBoxLayout;
    basicGroup->setLayout(basicGroupLayout);
    mainLayout->addWidget(basicGroup);

    QComboBox* backendSelect = new QComboBox;
    //TODO: Hardcoded
    backendSelect->addItem(tr("OpenGL"));
    backendSelect->addItem(tr("Software"));

    basicGroupLayout->addWidget(backendSelect);
}

GraphicsPage::GraphicsPage()
{
    mainLayout = new QVBoxLayout;
    BuildOptions();
    LoadConfig();
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}
