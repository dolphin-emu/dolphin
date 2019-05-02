// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

#include "Common/CommonTypes.h"

struct GCPadStatus;
class QBoxLayout;
class QCheckBox;
class QDialog;
class QGroupBox;
class QSpinBox;
class QString;
class TASCheckBox;

class TASInputWindow : public QDialog
{
  Q_OBJECT
public:
  explicit TASInputWindow(QWidget* parent);

protected:
  QGroupBox* CreateStickInputs(QString name, QSpinBox*& x_value, QSpinBox*& y_value, u16 max_x,
                               u16 max_y, Qt::Key x_shortcut_key, Qt::Key y_shortcut_key);
  QBoxLayout* CreateSliderValuePairLayout(QString name, QSpinBox*& value, u16 max,
                                          Qt::Key shortcut_key, QWidget* shortcut_widget,
                                          bool invert = false);
  QSpinBox* CreateSliderValuePair(QBoxLayout* layout, u16 max, QKeySequence shortcut_key_sequence,
                                  Qt::Orientation orientation, QWidget* shortcut_widget,
                                  bool invert = false);
  template <typename UX>
  void GetButton(TASCheckBox* button, UX& pad, UX mask);
  void GetSpinBoxU8(QSpinBox* spin, u8& controller_value);
  void GetSpinBoxU16(QSpinBox* spin, u16& controller_value);
  QCheckBox* m_use_controller;

private:
  std::map<TASCheckBox*, bool> m_checkbox_set_by_controller;
  std::map<QSpinBox*, u8> m_spinbox_most_recent_values_u8;
  std::map<QSpinBox*, u8> m_spinbox_most_recent_values_u16;
};
