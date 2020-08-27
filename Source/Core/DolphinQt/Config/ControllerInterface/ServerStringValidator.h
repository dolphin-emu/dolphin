// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QValidator>

class ServerStringValidator : public QValidator
{
  Q_OBJECT
public:
  explicit ServerStringValidator(QObject* parent);

  State validate(QString& input, int& pos) const override;
};
