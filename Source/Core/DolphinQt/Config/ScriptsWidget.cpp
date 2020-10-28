// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ScriptsWidget.h"

#include <QGridLayout>
#include <QListWidget>
#include <QPushButton>

ScriptsWidget::ScriptsWidget(const UICommon::GameFile& game)
{
  CreateWidgets();
  ConnectWidgets();

  Update();

  UpdateActions();
}

ScriptsWidget::~ScriptsWidget() = default;

void ScriptsWidget::CreateWidgets()
{
  m_list = new QListWidget;
  m_add_button = new QPushButton(tr("&Add..."));
  m_remove_button = new QPushButton(tr("&Remove"));

  auto* layout = new QGridLayout;

  layout->addWidget(m_list, 0, 0, 1, -1);
  layout->addWidget(m_add_button, 1, 0);
  layout->addWidget(m_remove_button, 1, 1);

  setLayout(layout);
}

void ScriptsWidget::ConnectWidgets()
{
  connect(m_list, &QListWidget::itemSelectionChanged, this, &ScriptsWidget::UpdateActions);
  connect(m_list, &QListWidget::itemChanged, this, &ScriptsWidget::OnItemChanged);
  connect(m_remove_button, &QPushButton::clicked, this, &ScriptsWidget::OnRemove);
  connect(m_add_button, &QPushButton::clicked, this, &ScriptsWidget::OnAdd);
}

void ScriptsWidget::OnItemChanged(QListWidgetItem* item)
{
}

void ScriptsWidget::OnAdd()
{
}

void ScriptsWidget::OnRemove()
{
  if (m_list->selectedItems().isEmpty())
    return;
  Update();
}

void ScriptsWidget::Update()
{
  m_list->clear();
}

void ScriptsWidget::UpdateActions()
{
  bool selected = !m_list->selectedItems().isEmpty();

  auto* item = selected ? m_list->selectedItems()[0] : nullptr;

  bool user_defined = selected ? item->data(Qt::UserRole).toBool() : true;

  m_remove_button->setEnabled(selected && user_defined);
}
