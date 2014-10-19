// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "ConfigDialog.h"
#include "ui_ConfigDialog.h"
#include "Common/Common.h"
#include "Common/StdMakeUnique.h"

DConfigDialog::DConfigDialog(QWidget* p)
	: QDialog(p)
{
	ui = std::make_unique<Ui::DConfigDialog>();
	ui->setupUi(this);

	CPUEngineButtonGroup = new QButtonGroup(this);
	CPUEngineButtonGroup->addButton(ui->cpuEngineInterpreterRadioButton);
	CPUEngineButtonGroup->addButton(ui->cpuEngineJitRadioButton);
	CPUEngineButtonGroup->addButton(ui->cpuEngineJitilRadioButton);
}

DConfigDialog::~DConfigDialog()
{
	delete CPUEngineButtonGroup;
}
