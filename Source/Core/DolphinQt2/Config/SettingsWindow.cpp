// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Config/SettingsWindow.h"

SettingsWindow::SettingsWindow(QWidget* parent)
    : QDialog(parent)
{
    // Set Window Properties
    setWindowTitle(tr("Settings"));
    resize(720, 600);

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

        MakeUnfinishedWarning();

        content_inner->addWidget(m_warning_group);
        content_inner->addWidget(m_settings_outer);

        content->addLayout(content_inner);
    }

    // Add content to layout before dialog buttons.
    layout->addLayout(content);

    // Dialog box buttons
    QDialogButtonBox* ok_box = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(ok_box, &QDialogButtonBox::accepted, this, &SettingsWindow::accept);
    layout->addWidget(ok_box);

    setLayout(layout);
}

void SettingsWindow::SetupSettingsWidget()
{
    m_settings_outer = new QStackedWidget;
    m_settings_outer->setCurrentIndex(0);
}

void SettingsWindow::MakeUnfinishedWarning()
{
    m_warning_group = new QGroupBox(tr("Warning"));
    QHBoxLayout* m_warning_group_layout = new QHBoxLayout;
    QLabel* warning_text = new QLabel(
            tr("Some categories and settings will not work.\n"
                "This Settings Window is under active development."));
    m_warning_group_layout->addWidget(warning_text);
    m_warning_group->setLayout(m_warning_group_layout);
}

void SettingsWindow::AddCategoryToList(const QString& title, const QString& icon)
{
    QListWidgetItem* button = new QListWidgetItem();
    button->setIcon(QIcon(icon));
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
    m_categories->setIconSize(QSize(32, 32));
    m_categories->setMovement(QListView::Static);
    m_categories->setSpacing(0);

    connect(m_categories, &QListWidget::currentItemChanged, this, &SettingsWindow::changePage);
}

void SettingsWindow::changePage(QListWidgetItem* current, QListWidgetItem* previous)
{
    if (!current)
        current = previous;
    m_settings_outer->setCurrentIndex(m_categories->row(current));
}
