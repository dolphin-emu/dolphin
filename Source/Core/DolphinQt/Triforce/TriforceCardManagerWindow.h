// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include <QString>
#include <QWidget>

#include "Common/CommonTypes.h"

class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QTimer;

namespace Core
{
enum class State;
}

class TriforceCardManagerWindow final : public QWidget
{
  Q_OBJECT

public:
  explicit TriforceCardManagerWindow(QWidget* parent = nullptr);
  ~TriforceCardManagerWindow() override;

  static void RestoreStoredCardsForRunningGame();

private:
  void CreateMainWindow();
  void RefreshUi();
  void OnEmulationStateChanged(Core::State state);
  void OnGameChanged();
  void OpenCardFolder();
  void CreateCard(u32 slot_index);
  void SetSelectedCard(u32 slot_index);
  void SetSlotInserted(u32 slot_index, bool inserted);

  struct SlotWidgets
  {
    QGroupBox* group = nullptr;
    QComboBox* combo = nullptr;
    QPushButton* create = nullptr;
    QPushButton* insert = nullptr;
    QPushButton* pull = nullptr;
    QLabel* status = nullptr;
  };

  std::array<SlotWidgets, 2> m_slots{};
  QComboBox* m_game = nullptr;
  QTimer* m_refresh_timer = nullptr;
};
