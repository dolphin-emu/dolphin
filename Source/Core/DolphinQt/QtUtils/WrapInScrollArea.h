// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class QLayout;
class QWidget;

// Puts the given widget in a QScrollArea and returns that.
QWidget* GetWrappedWidget(QWidget* wrapped_widget);

// Wrap wrapped_layout in a QScrollArea and fill the parent widget with it.
void WrapInScrollArea(QWidget* parent, QLayout* wrapped_layout);
