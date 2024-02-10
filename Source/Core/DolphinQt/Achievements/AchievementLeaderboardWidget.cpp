// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef USE_RETRO_ACHIEVEMENTS
#include "DolphinQt/Achievements/AchievementLeaderboardWidget.h"

#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QString>
#include <QVBoxLayout>

#include "Common/CommonTypes.h"
#include "Core/AchievementManager.h"
#include "Core/Config/AchievementSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Core.h"

#include "DolphinQt/QtUtils/ClearLayoutRecursively.h"
#include "DolphinQt/Settings.h"

AchievementLeaderboardWidget::AchievementLeaderboardWidget(QWidget* parent) : QWidget(parent)
{
  m_common_box = new QGroupBox();
  m_common_layout = new QGridLayout();

  {
    std::lock_guard lg{AchievementManager::GetInstance().GetLock()};
    UpdateData();
  }

  m_common_box->setLayout(m_common_layout);

  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_common_box);
  setLayout(layout);
}

void AchievementLeaderboardWidget::UpdateData()
{
  ClearLayoutRecursively(m_common_layout);

  if (!AchievementManager::GetInstance().IsGameLoaded())
    return;
  const auto& leaderboards = AchievementManager::GetInstance().GetLeaderboardsInfo();
  int row = 0;
  for (const auto& board_row : leaderboards)
  {
    const AchievementManager::LeaderboardStatus& board = board_row.second;
    QLabel* a_title = new QLabel(QString::fromStdString(board.name));
    QLabel* a_description = new QLabel(QString::fromStdString(board.description));
    QVBoxLayout* a_col_left = new QVBoxLayout();
    a_col_left->addWidget(a_title);
    a_col_left->addWidget(a_description);
    if (row > 0)
    {
      QFrame* a_divider = new QFrame();
      a_divider->setFrameShape(QFrame::HLine);
      m_common_layout->addWidget(a_divider, row - 1, 0);
    }
    m_common_layout->addLayout(a_col_left, row, 0);
    // Each leaderboard entry is displayed with four values. These are *generally* intended to be,
    // in order, the first place entry, the entry one above the player, the player's entry, and
    // the entry one below the player.
    // Edge cases:
    // * If there are fewer than four entries in the leaderboard, all entries will be shown in
    //   order and the remainder of the list will be padded with empty values.
    // * If the player does not currently have a score in the leaderboard, or is in the top 3,
    //   the four slots will be the top four players in order.
    // * If the player is last place, the player will be in the fourth slot, and the second and
    //   third slots will be the two players above them. The first slot will always be first place.
    std::array<u32, 4> to_display{1, 2, 3, 4};
    if (board.player_index > to_display.size() - 1)
    {
      // If the rank one below than the player is found, offset = 1.
      u32 offset = static_cast<u32>(board.entries.count(board.player_index + 1));
      // Example: player is 10th place but not last
      // to_display = {1, 10-3+1+1, 10-3+1+2, 10-3+1+3} = {1, 9, 10, 11}
      // Example: player is 15th place and is last
      // to_display = {1, 15-3+0+1, 15-3+0+2, 15-3+0+3} = {1, 13, 14, 15}
      for (size_t i = 1; i < to_display.size(); ++i)
        to_display[i] = board.player_index - 3 + offset + static_cast<u32>(i);
    }
    for (size_t i = 0; i < to_display.size(); ++i)
    {
      u32 index = to_display[i];
      QLabel* a_rank = new QLabel(QStringLiteral("---"));
      QLabel* a_username = new QLabel(QStringLiteral("---"));
      QLabel* a_score = new QLabel(QStringLiteral("---"));
      const auto it = board.entries.find(index);
      if (it != board.entries.end())
      {
        a_rank->setText(tr("Rank %1").arg(it->second.rank));
        a_username->setText(QString::fromStdString(it->second.username));
        a_score->setText(QString::fromUtf8(it->second.score.data()));
      }
      QVBoxLayout* a_col = new QVBoxLayout();
      a_col->addWidget(a_rank);
      a_col->addWidget(a_username);
      a_col->addWidget(a_score);
      if (row > 0)
      {
        QFrame* a_divider = new QFrame();
        a_divider->setFrameShape(QFrame::HLine);
        m_common_layout->addWidget(a_divider, row - 1, static_cast<int>(i) + 1);
      }
      m_common_layout->addLayout(a_col, row, static_cast<int>(i) + 1);
    }
    row += 2;
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
