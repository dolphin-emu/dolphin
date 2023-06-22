// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QValidator>

class ServerStringValidator : public QValidator
{
  Q_OBJECT
public:
  explicit ServerStringValidator(QObject* parent);

  State validate(QString& input, int& pos) const override;
};
