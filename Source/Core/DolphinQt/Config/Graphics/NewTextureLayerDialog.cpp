// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/Graphics/NewTextureLayerDialog.h"

#include <QBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>

NewTextureLayerDialog::NewTextureLayerDialog(QWidget* parent) : QDialog(parent)
{
  CreateMainLayout();

  setWindowTitle(tr("New Texture Layer Dialog"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

void NewTextureLayerDialog::CreateMainLayout()
{
  m_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(m_button_box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_button_box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto* main_layout = new QVBoxLayout();

  auto* hlayout = new QHBoxLayout;
  m_texture_layer_name_edit = new QLineEdit();
  hlayout->addWidget(new QLabel(tr("Texture Layer name: ")));
  hlayout->addWidget(m_texture_layer_name_edit);
  main_layout->addItem(hlayout);
  main_layout->addWidget(m_button_box);
  setLayout(main_layout);
}
