// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QLineEdit;

class BroadbandAdapterSettingsDialog final : public QDialog
{
  Q_OBJECT
public:
  enum class Type
  {
    Ethernet,
    XLinkKai,
    TapServer,
    BuiltIn,
    ModemTapServer
  };

  explicit BroadbandAdapterSettingsDialog(QWidget* parent, Type bba_type);

private:
  QLineEdit* m_address_input;
  Type m_bba_type;

  void InitControls();
  void SaveAddress();
};
