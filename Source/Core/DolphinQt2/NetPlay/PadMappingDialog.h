// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Core/NetPlayProto.h"

class NetPlayClient;
class Player;
class QGridLayout;
class QComboBox;
class QDialogButtonBox;

class PadMappingDialog : public QDialog
{
  Q_OBJECT
public:
  explicit PadMappingDialog(QWidget* widget);

  int exec();

  PadMappingArray GetGCPadArray();
  PadMappingArray GetWiimoteArray();

private:
  void CreateWidgets();
  void ConnectWidgets();
  void OnMappingChanged();

  PadMappingArray m_pad_mapping;
  PadMappingArray m_wii_mapping;

  QGridLayout* m_main_layout;
  std::array<QComboBox*, 4> m_gc_boxes;
  std::array<QComboBox*, 4> m_wii_boxes;
  std::vector<const Player*> m_players;
  QDialogButtonBox* m_button_box;
};
