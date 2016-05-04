#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSize>
#include <QSizePolicy>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "SettingsPages.h"

GraphicsPage::GraphicsPage()
{
    QGroupBox *configGroup = new QGroupBox(tr("Graphics"));
    QVBoxLayout *configLayout = new QVBoxLayout;

    {
        QGroupBox *basicGroup = new QGroupBox(tr("Basic"));
        configLayout->addWidget(basicGroup);

        QComboBox *backendSelect = new QComboBox;
        //TODO: Hardcoded
        backendSelect->addItem(tr("OpenGL"));
        backendSelect->addItem(tr("Software"));

        QVBoxLayout *basicGroupLayout = new QVBoxLayout;

        basicGroupLayout->addWidget(backendSelect);

        basicGroup->setLayout(basicGroupLayout);
    }

    configGroup->setLayout(configLayout);
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(configGroup);
    mainLayout->addStretch(1);
    setLayout(mainLayout);
}