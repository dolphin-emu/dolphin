// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/FreeLook2DOptionsWidget.h"

#include <QBoxLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QPushButton>

#include "Common/Config/Config.h"
#include "Core/Config/FreeLookSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "DolphinQt/Config/Graphics/TextureLayersWidget.h"

FreeLook2DOptionsWidget::FreeLook2DOptionsWidget(QWidget* parent) : QWidget(parent)
{
  CreateLayout();
}

void FreeLook2DOptionsWidget::CreateLayout()
{
  auto* main_layout = new QVBoxLayout;

  QGroupBox* texture_layers_group = new QGroupBox(tr("Texture layers"));
  auto* groupbox_layout = new QVBoxLayout;
  groupbox_layout->addWidget(new TextureLayersWidget(this, Config::FL1_2DLAYERS));
  texture_layers_group->setLayout(groupbox_layout);
  main_layout->addWidget(texture_layers_group);

  setLayout(main_layout);
}
