// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "DolphinQt2/Config/SettingsWindow.h"
#include "DolphinQt2/MainWindow.h"
#include "DolphinQt2/Resources.h"
#include "DolphinQt2/Settings.h"
#include "DolphinQt2/Settings/AudioPane.h"
#include "DolphinQt2/Settings/GeneralPane.h"
#include "DolphinQt2/Settings/InterfacePane.h"
#include "DolphinQt2/Settings/PathPane.h"

SettingsWindow::SettingsWindow(QWidget* parent) : QDialog(parent)
{
  // Set Window Properties
  setWindowTitle(tr("Settings"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  resize(720, 600);

  // Main Layout
  QVBoxLayout* layout = new QVBoxLayout;
  QHBoxLayout* content = new QHBoxLayout;
  // Content's widgets
  {
    // Category list
    MakeCategoryList();
    content->addWidget(m_categories);

    // Actual Settings UI
    SetupSettingsWidget();

    content->addWidget(m_settings_outer);
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

  // Panes initalised here
  m_settings_outer->addWidget(new GeneralPane);
  m_settings_outer->addWidget(new InterfacePane);

  auto* audio_pane = new AudioPane;
  connect(this, &SettingsWindow::EmulationStarted,
          [audio_pane] { audio_pane->OnEmulationStateChanged(true); });
  connect(this, &SettingsWindow::EmulationStopped,
          [audio_pane] { audio_pane->OnEmulationStateChanged(false); });
  m_settings_outer->addWidget(audio_pane);

  m_settings_outer->addWidget(new PathPane);
}

void SettingsWindow::AddCategoryToList(const QString& title, const std::string& icon_name)
{
  QListWidgetItem* button = new QListWidgetItem();
  button->setText(title);
  button->setTextAlignment(Qt::AlignVCenter);
  button->setSizeHint(QSize(28, 28));
  button->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

  auto set_icon = [=] { button->setIcon(Resources::GetScaledThemeIcon(icon_name)); };
  QObject::connect(&Settings::Instance(), &Settings::ThemeChanged, set_icon);
  set_icon();
  m_categories->addItem(button);
}

void SettingsWindow::MakeCategoryList()
{
  m_categories = new QListWidget;
  m_categories->setMaximumWidth(175);
  m_categories->setIconSize(QSize(32, 32));
  m_categories->setMovement(QListView::Static);
  m_categories->setSpacing(0);

  AddCategoryToList(tr("General"), "config");
  AddCategoryToList(tr("Interface"), "browse");
  AddCategoryToList(tr("Audio"), "play");
  AddCategoryToList(tr("Paths"), "browse");
  connect(m_categories, &QListWidget::currentItemChanged, this, &SettingsWindow::changePage);
}

void SettingsWindow::changePage(QListWidgetItem* current, QListWidgetItem* previous)
{
  if (!current)
    current = previous;
  m_settings_outer->setCurrentIndex(m_categories->row(current));
}
