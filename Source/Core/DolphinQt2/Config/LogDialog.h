// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QObject>

#include "Common/IniFile.h"
#include "Common/Logging/LogManager.h"
#include "DolphinQt2/LogViewer.h"

class LogDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit LogDialog(QWidget* parent = nullptr);

public slots:
};
