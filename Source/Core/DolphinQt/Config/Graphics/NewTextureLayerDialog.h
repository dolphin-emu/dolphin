// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QLineEdit>

class QDialogButtonBox;

class NewTextureLayerDialog final : public QDialog
{
  Q_OBJECT
public:
  NewTextureLayerDialog(QWidget* parent);

  QString GetTextureLayerName() const { return m_texture_layer_name_edit->text(); }

private:
  void CreateMainLayout();

  QLineEdit* m_texture_layer_name_edit;
  QDialogButtonBox* m_button_box;
};
