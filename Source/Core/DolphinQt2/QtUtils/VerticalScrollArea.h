// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QScrollArea>

// A scroll area that automatically adjusts itself to the child's width, and only ever shows
// scrollbars on the vertical axis (if needed).
class VerticalScrollArea : public QScrollArea
{
public:
  explicit VerticalScrollArea(QWidget* parent = nullptr);
  QSize sizeHint() const override;
};

// Takes a given QLayout and wraps it in a VerticalScrollArea. The returned QLayout has no extra
// margins and no frame, but will show a vertical scroll bar if needed. Use this, for example, to
// wrap a dialog's contents if they might be taller than the screen size.
QLayout* WrapInVerticalScrollArea(QLayout* layout);
