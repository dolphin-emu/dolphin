// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QAction>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSize>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QVBoxLayout>
#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Config/SettingsPages.h"
#include "DolphinQt2/Config/SettingsWindow.h"

SettingsWindow::SettingsWindow(QWidget* parent)
    : QDialog(parent)
{
    // Set Window Properties
    setWindowTitle(tr("Settings"));
    resize(640,480);

    // Main Layout
    QVBoxLayout* layout = new QVBoxLayout;
    QHBoxLayout* content = new QHBoxLayout;
    QVBoxLayout* content_inner = new QVBoxLayout;
    // Content's widgets
    {
        // Category list
        MakeCategoryList();
        content->addWidget(m_categories);
        // Actual Settings UI
        SetupSettingsWidget();

        content_inner->addWidget(m_settings_outer);

        content->addLayout(content_inner,0);
    }

    // Add content to layout before dialog buttons.
    layout->addLayout(content, 0);

    // Dialog box buttons
    QDialogButtonBox* ok_box = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(ok_box, &QDialogButtonBox::accepted, this, &SettingsWindow::accept);
    layout->addWidget(ok_box);

    setLayout(layout);
}

void SettingsWindow::SetupSettingsWidget()
{
    m_settings_outer = new QStackedWidget;

    m_settings_outer->addWidget(new GeneralPage);
    m_settings_outer->addWidget(new GraphicsPage);
    m_settings_outer->setCurrentIndex(0);

}

void SettingsWindow::MakeCategoryList()
{
    QString dir = Settings().GetThemeDir();
    m_categories = new QListWidget;
    m_categories->setMaximumWidth(150);
    //m_categories->setViewMode(QListView::IconMode);
    m_categories->setIconSize(QSize(32, 32));
    m_categories->setMovement(QListView::Static);
    m_categories->setSpacing(2);

    // General
    QListWidgetItem* generalButton = new QListWidgetItem();
    generalButton->setIcon(QIcon(QStringLiteral("config.png").prepend(dir)));
    generalButton->setText(tr("General"));
    generalButton->setTextAlignment(Qt::AlignVCenter);
    generalButton->setSizeHint(QSize(28, 28));
    generalButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    m_categories->addItem(generalButton);

    // General
    QListWidgetItem* graphicsButton = new QListWidgetItem();
    graphicsButton->setIcon(QIcon(QStringLiteral("graphics.png").prepend(dir)));
    graphicsButton->setText(tr("Graphics"));
    graphicsButton->setTextAlignment(Qt::AlignVCenter);
    graphicsButton->setSizeHint(QSize(28, 28));
    graphicsButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    m_categories->addItem(graphicsButton);

    // Interface
    QListWidgetItem* interfaceButton = new QListWidgetItem();
    interfaceButton->setIcon(QIcon(QStringLiteral("fullscreen.png").prepend(dir)));
    interfaceButton->setText(tr("Interface"));
    interfaceButton->setTextAlignment(Qt::AlignVCenter);
    interfaceButton->setSizeHint(QSize(28, 28));
    interfaceButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    m_categories->addItem(interfaceButton);

    // Audio
    QListWidgetItem* audioButton = new QListWidgetItem();
    audioButton->setIcon(QIcon(QStringLiteral("play.png").prepend(dir)));
    audioButton->setText(tr("Audio"));
    audioButton->setTextAlignment(Qt::AlignVCenter);
    audioButton->setSizeHint(QSize(28, 28));
    audioButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    m_categories->addItem(audioButton);

    // Gamecube
    QListWidgetItem* gamecubeButton = new QListWidgetItem();
    gamecubeButton->setIcon(QIcon(QStringLiteral("gcpad.png").prepend(dir)));
    gamecubeButton->setText(tr("Gamecube"));
    gamecubeButton->setTextAlignment(Qt::AlignVCenter);
    gamecubeButton->setSizeHint(QSize(28, 28));
    gamecubeButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    m_categories->addItem(gamecubeButton);

    // Wii
    QListWidgetItem* wiiButton = new QListWidgetItem();
    wiiButton->setIcon(QIcon(QStringLiteral("wiimote.png").prepend(dir)));
    wiiButton->setText(tr("Wii"));
    wiiButton->setTextAlignment(Qt::AlignVCenter);
    wiiButton->setSizeHint(QSize(28, 28));
    wiiButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    m_categories->addItem(wiiButton);

    // Paths
    QListWidgetItem* pathsButton = new QListWidgetItem();
    pathsButton->setIcon(QIcon(QStringLiteral("browse.png").prepend(dir)));
    pathsButton->setText(tr("Paths"));
    pathsButton->setTextAlignment(Qt::AlignVCenter);
    pathsButton->setSizeHint(QSize(28, 28));
    pathsButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    m_categories->addItem(pathsButton);

    // Advanced
    QListWidgetItem* advancedButton = new QListWidgetItem();
    advancedButton->setIcon(QIcon(QStringLiteral("config.png").prepend(dir)));
    advancedButton->setText(tr("Advanced"));
    advancedButton->setTextAlignment(Qt::AlignVCenter);
    advancedButton->setSizeHint(QSize(28, 28));
    advancedButton->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    m_categories->addItem(advancedButton);

    connect(m_categories, &QListWidget::currentItemChanged, this, &SettingsWindow::changePage);
}

void SettingsWindow::changePage(QListWidgetItem *current, QListWidgetItem *previous)
{
    if (!current)
        current = previous;
    // TODO: Save the page we're switching out of.
    // And swap.
    m_settings_outer->setCurrentIndex(m_categories->row(current));
}