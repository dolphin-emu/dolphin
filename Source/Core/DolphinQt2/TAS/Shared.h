// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

#include <QWidget>

class QDialog;
class QString;
class QSpinBox;
class QCheckBox;
class QBoxLayout;
class QGroupBox;
struct GCPadStatus;

QGroupBox* CreateStickInputs(QDialog* window, QString name, QSpinBox*& x_value, QSpinBox*& y_value,
                             u16 max_x, u16 max_y, Qt::Key x_shortcut_key, Qt::Key y_shortcut_key);
QBoxLayout* CreateSliderValuePairLayout(QDialog* window, QString name, QSpinBox*& value, u16 max,
                                        Qt::Key shortcut_key, QWidget* shortcut_widget);
QSpinBox* CreateSliderValuePair(QDialog* window, QBoxLayout* layout, u16 max, Qt::Key shortcut_key,
                                Qt::Orientation orientation, QWidget* shortcut_widget);
