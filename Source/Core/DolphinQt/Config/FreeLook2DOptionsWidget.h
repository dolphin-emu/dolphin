// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QWidget>

class QGroupBox;

class FreeLook2DOptionsWidget final : public QWidget
{
  Q_OBJECT
public:
  explicit FreeLook2DOptionsWidget(QWidget* parent);

private:
  void CreateLayout();

  QGroupBox* m_texture_layer_group_box;
};
