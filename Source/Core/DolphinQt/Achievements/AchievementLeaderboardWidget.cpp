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

  m_common_box->setLayout(m_common_layout);

  auto* layout = new QVBoxLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setAlignment(Qt::AlignTop);
  layout->addWidget(m_common_box);
  setLayout(layout);
}

void AchievementLeaderboardWidget::UpdateData(bool clean_all)
{
  if (clean_all)
  {
    ClearLayoutRecursively(m_common_layout);

    auto& instance = AchievementManager::GetInstance();
    if (!instance.IsGameLoaded())
      return;
    auto* client = instance.GetClient();
    auto* leaderboard_list =
        rc_client_create_leaderboard_list(client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);

    u32 row = 0;
    for (u32 bucket = 0; bucket < leaderboard_list->num_buckets; bucket++)
    {
      const auto& leaderboard_bucket = leaderboard_list->buckets[bucket];
      m_common_layout->addWidget(new QLabel(tr(leaderboard_bucket.label)));
      for (u32 board = 0; board < leaderboard_bucket.num_leaderboards; board++)
      {
        const auto* leaderboard = leaderboard_bucket.leaderboards[board];
        m_leaderboard_order[leaderboard->id] = row;
        QLabel* a_title = new QLabel(QString::fromUtf8(leaderboard->title));
        a_title->setWordWrap(true);
        a_title->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        QLabel* a_description = new QLabel(QString::fromUtf8(leaderboard->description));
        a_description->setWordWrap(true);
        a_description->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
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
        for (size_t ix = 0; ix < 4; ix++)
        {
          QVBoxLayout* a_col = new QVBoxLayout();
          for (size_t jx = 0; jx < 3; jx++)
            a_col->addWidget(new QLabel(QStringLiteral("---")));
          if (row > 0)
          {
            QFrame* a_divider = new QFrame();
            a_divider->setFrameShape(QFrame::HLine);
            m_common_layout->addWidget(a_divider, row - 1, static_cast<int>(ix) + 1);
          }
          m_common_layout->addLayout(a_col, row, static_cast<int>(ix) + 1);
        }
        row += 2;
      }
    }
    rc_client_destroy_leaderboard_list(leaderboard_list);
  }
  for (auto row : m_leaderboard_order)
  {
    UpdateRow(row.second);
  }
}

void AchievementLeaderboardWidget::UpdateData(
    const std::set<AchievementManager::AchievementId>& update_ids)
{
  for (auto row : m_leaderboard_order)
  {
    if (update_ids.contains(row.first))
    {
      UpdateRow(row.second);
    }
  }
}

void AchievementLeaderboardWidget::UpdateRow(AchievementManager::AchievementId leaderboard_id)
{
  const auto leaderboard_itr = m_leaderboard_order.find(leaderboard_id);
  if (leaderboard_itr == m_leaderboard_order.end())
    return;
  const int row = leaderboard_itr->second;

  const AchievementManager::LeaderboardStatus* board;
  {
    std::lock_guard lg{AchievementManager::GetInstance().GetLock()};
    board = AchievementManager::GetInstance().GetLeaderboardInfo(leaderboard_id);
  }
  if (!board)
    return;

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
  if (board->player_index > to_display.size() - 1)
  {
    // If the rank one below than the player is found, offset = 1.
    u32 offset = static_cast<u32>(board->entries.count(board->player_index + 1));
    // Example: player is 10th place but not last
    // to_display = {1, 10-3+1+1, 10-3+1+2, 10-3+1+3} = {1, 9, 10, 11}
    // Example: player is 15th place and is last
    // to_display = {1, 15-3+0+1, 15-3+0+2, 15-3+0+3} = {1, 13, 14, 15}
    for (size_t ix = 1; ix < to_display.size(); ++ix)
      to_display[ix] = board->player_index - 3 + offset + static_cast<u32>(ix);
  }
  for (size_t ix = 0; ix < to_display.size(); ++ix)
  {
    const auto it = board->entries.find(to_display[ix]);
    if (it != board->entries.end())
    {
      QVBoxLayout* a_col = new QVBoxLayout();
      a_col->addWidget(new QLabel(tr("Rank %1").arg(it->second.rank)));
      a_col->addWidget(new QLabel(QString::fromStdString(it->second.username)));
      a_col->addWidget(new QLabel(QString::fromUtf8(it->second.score.data())));
      auto old_item = m_common_layout->itemAtPosition(row, static_cast<int>(ix) + 1);
      m_common_layout->removeItem(old_item);
      ClearLayoutRecursively(static_cast<QLayout*>(old_item));
      m_common_layout->addLayout(a_col, row, static_cast<int>(ix) + 1);
    }
  }
}

#endif  // USE_RETRO_ACHIEVEMENTS
