// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Config/GeneralPage.h"
#include "DolphinQt2/Config/GraphicsPage.h"
#include "DolphinQt2/Config/InterfacePage.h"
#include "DolphinQt2/Config/SettingsPages.h"
#include "DolphinQt2/Config/SettingsWindow.h"

class QAction;
class QDialogButtonBox;
class QDir;
class QDialog;
class QFileDialog;
class QFont;
class QLabel;
class QListWidget;
class QPushButton;
class QSize;
class QSizePolicy;
class QStackedWidget;
class QVBoxLayout;

SettingsWindow::SettingsWindow(QWidget* parent)
    : QDialog(parent)
{
    // Set Window Properties
    setWindowTitle(tr("Settings"));
    resize(720,600);

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
    m_settings_outer->addWidget(new InterfacePage);
    m_settings_outer->addWidget(new GraphicsPage);
    m_settings_outer->setCurrentIndex(0);

}

void SettingsWindow::AddCategoryToList(QString title, QString icon)
{
    QString dir = Settings().GetThemeDir();

    QListWidgetItem* button = new QListWidgetItem();
    button->setIcon(QIcon(icon.prepend(dir)));
    button->setText(title);
    button->setTextAlignment(Qt::AlignVCenter);
    button->setSizeHint(QSize(28, 28));
    button->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    m_categories->addItem(button);
}

void SettingsWindow::MakeCategoryList()
{
    QString dir = Settings().GetThemeDir();
    m_categories = new QListWidget;
    m_categories->setMaximumWidth(175);
    //m_categories->setViewMode(QListView::IconMode);
    m_categories->setIconSize(QSize(32, 32));
    m_categories->setMovement(QListView::Static);
    m_categories->setSpacing(0);

    AddCategoryToList(tr("General"),QStringLiteral("config.png"));
    AddCategoryToList(tr("Interface"),QStringLiteral("browse.png"));
    AddCategoryToList(tr("Graphics"),QStringLiteral("graphics.png"));
    AddCategoryToList(tr("Enhancements"),QStringLiteral("screenshot.png"));
    AddCategoryToList(tr("CPU"),QStringLiteral("refresh.png"));
    AddCategoryToList(tr("Audio"),QStringLiteral("play.png"));
    AddCategoryToList(tr("GameCube"),QStringLiteral("gcpad.png"));
    AddCategoryToList(tr("Wii"),QStringLiteral("wiimote.png"));
    AddCategoryToList(tr("Data"),QStringLiteral("open.png"));
    AddCategoryToList(tr("Textures"),QStringLiteral("stop.png"));
    AddCategoryToList(tr("Hacks"),QStringLiteral("config.png"));
    AddCategoryToList(tr("Debug"),QStringLiteral("refresh.png"));

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