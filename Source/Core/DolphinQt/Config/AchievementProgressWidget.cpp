// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// 25 JAN 2023 - Lilly Jade Katrin - lilly.kitty.1988@gmail.com
// Thanks to Stenzek and the PCSX2 project for inspiration, assistance and examples,
// to TheFetishMachine and Infernum for encouragement and cheerleading,
// and to Gollawiz, Sea, Fridge, jenette and Ryudo for testing

#include "DolphinQt/Config/AchievementProgressWidget.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include <rcheevos/include/rc_api_runtime.h>
#include <rcheevos/include/rc_api_user.h>
#include "rcheevos/include/rc_runtime.h"

#include "Core/AchievementManager.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"

#include <Core/Config/AchievementSettings.h>
#include <ModalMessageBox.h>
#include <QProgressBar>
#include "DolphinQt/Config/ControllerInterface/ControllerInterfaceWindow.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "DolphinQt/QtUtils/SignalBlocking.h"
#include "DolphinQt/Settings.h"

AchievementProgressWidget::AchievementProgressWidget(QWidget* parent) : QWidget(parent)
{
  m_common_box = new QGroupBox();
  m_common_layout = new QVBoxLayout();

  for (unsigned int ix = 0; ix < Achievements::GetGameData()->num_achievements; ix++)
  {
    m_common_layout->addWidget(
        CreateAchievementBox(Achievements::GetGameData()->achievements + ix));
  }

  m_common_box->setLayout(m_common_layout);

  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_common_box);
  setLayout(layout);
}

std::string GetAchievementColor(unsigned int id)
{
  bool uploaded_hardcore = false;
  for (unsigned int ix = 0; ix < Achievements::GetHardcoreGameProgress()->num_achievement_ids; ix++)
  {
    if (Achievements::GetHardcoreGameProgress()->achievement_ids[ix] == id)
    {
      uploaded_hardcore = true;
      continue;
    }
  }
  bool uploaded_softcore = false;
  for (unsigned int ix = 0; ix < Achievements::GetSoftcoreGameProgress()->num_achievement_ids; ix++)
  {
    if (Achievements::GetSoftcoreGameProgress()->achievement_ids[ix] == id)
    {
      uploaded_softcore = true;
      continue;
    }
  }
  if (Achievements::GetHardcoreAchievementStatus(id) > 0 || uploaded_hardcore)
    return Achievements::GOLD;
  if (Achievements::GetSoftcoreAchievementStatus(id) > 0 || uploaded_softcore)
    return Achievements::BLUE;
  return Achievements::GRAY;
}

QGroupBox*
AchievementProgressWidget::CreateAchievementBox(const rc_api_achievement_definition_t* achievement)
{
  std::string color = GetAchievementColor(achievement->id);
  QImage i_icon;
  i_icon.loadFromData(
      Achievements::GetAchievementBadge(achievement->id, color == Achievements::GRAY)->begin()._Ptr,
      (int)Achievements::GetAchievementBadge(achievement->id, color == Achievements::GRAY)->size());
  QLabel* a_icon = new QLabel();
  a_icon->setPixmap(QPixmap::fromImage(i_icon));
  a_icon->adjustSize();
  a_icon->setFixedWidth(72);
  a_icon->setStyleSheet(QString::fromStdString(std::format("border: 4px solid {}", color)));
  QLabel* a_title =
      new QLabel(QString::fromLocal8Bit(achievement->title, strlen(achievement->title)));
  QLabel* a_description = new QLabel(
      QString::fromLocal8Bit(achievement->description, strlen(achievement->description)));
  QLabel* a_points =
      new QLabel(QString::fromStdString(std::format("{} points", achievement->points)));
  QProgressBar* a_progress_bar = new QProgressBar();
  unsigned int value = 0;
  unsigned int target = 0;
  Achievements::GetAchievementProgress(achievement->id, &value, &target);
  if (target > 0)
  {
    a_progress_bar->setRange(0, target);
    a_progress_bar->setValue(value);
  }
  else
  {
    a_progress_bar->setVisible(false);
  }

  QVBoxLayout* a_col_right = new QVBoxLayout();
  a_col_right->addWidget(a_title);
  a_col_right->addWidget(a_description);
  a_col_right->addWidget(a_points);
  a_col_right->addWidget(a_progress_bar);
  QHBoxLayout* a_total = new QHBoxLayout();
  a_total->addWidget(a_icon);
  a_total->addLayout(a_col_right);
  QGroupBox* a_group_box = new QGroupBox();
  a_group_box->setLayout(a_total);
  return a_group_box;
}

void AchievementProgressWidget::UpdateData()
{
  QLayoutItem* item;
  while ((item = m_common_layout->layout()->takeAt(0)) != NULL)
  {
    delete item->widget();
    delete item;
  }

  for (unsigned int ix = 0; ix < Achievements::GetGameData()->num_achievements; ix++)
  {
    m_common_layout->addWidget(
        CreateAchievementBox(Achievements::GetGameData()->achievements + ix));
  }
}
