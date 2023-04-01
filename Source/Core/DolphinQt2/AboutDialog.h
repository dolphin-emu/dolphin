// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class AboutDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit AboutDialog(QWidget* parent = nullptr);
};

