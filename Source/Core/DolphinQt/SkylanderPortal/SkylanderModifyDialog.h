// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"
#include "Core/IOS/USB/Emulated/Skylanders/SkylanderFigure.h"

class QVBoxLayout;
class QDialogButtonBox;
class QCheckBox;

class SkylanderModifyDialog : public QDialog
{
public:
  explicit SkylanderModifyDialog(QWidget* parent = nullptr, u8 slot = 0);

private:
  void PopulateSkylanderOptions(QVBoxLayout* layout);
  bool PopulateTrophyOptions(QVBoxLayout* layout);
  void accept() override;
  u32 ConvertSSAHeroicsToU32(std::span<QCheckBox* const> checkboxes);
  u16 ConvertSkillsToU16(std::span<QCheckBox* const> checkboxes);
  void ToggleAllSSAHeroics(std::span<QCheckBox* const> checkboxes);
  void ToggleSSAHeroicsGroup(std::span<QCheckBox* const> checkboxes,
                             const std::vector<u8>& indices);

  bool m_allow_close = false;
  u8 m_slot;
  IOS::HLE::USB::FigureData m_figure_data;
  IOS::HLE::USB::SkylanderFigure* m_figure;
  QDialogButtonBox* m_buttons;
};
