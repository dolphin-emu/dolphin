// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <QWidget>

#include "Common/CommonTypes.h"
#include "Common/Config/ConfigInfo.h"

class QLineEdit;
class QSpinBox;

class FreeLookUDPOptionsWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit FreeLookUDPOptionsWidget(QWidget* parent,
                                    const Config::Info<std::string>& udp_address_config,
                                    const Config::Info<u16>& udp_port_config);

private:
  void CreateLayout();
  void ConnectWidgets();

  void LoadSettings();
  void SaveSettings();

  const Config::Info<std::string>& m_udp_address_config;
  const Config::Info<u16>& m_udp_port_config;

  QLineEdit* m_server_address;
  QSpinBox* m_server_port;
};
