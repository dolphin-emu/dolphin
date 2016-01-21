// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <QDialog>
#include <QString>

#include "Common/IniFile.h"
#include "Common/Logging/LogManager.h"
#include "DolphinQt2/LogViewer.h"
#include "DolphinQt2/Config/LogDialog.h"

LogDialog::LogDialog(QWidget* parent)
	: QDialog(parent)
{
	setAttribute(Qt::WA_DeleteOnClose);
}
