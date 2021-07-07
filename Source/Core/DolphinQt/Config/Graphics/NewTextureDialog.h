// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QLineEdit>

class QDialogButtonBox;

class NewTextureDialog final : public QDialog
{
  Q_OBJECT
public:
  NewTextureDialog(QWidget* parent);

  QString GetTextureName() const { return m_texture_name_edit->text(); }

private:
  void CreateMainLayout();

  QLineEdit* m_texture_name_edit;
  QDialogButtonBox* m_button_box;
};
