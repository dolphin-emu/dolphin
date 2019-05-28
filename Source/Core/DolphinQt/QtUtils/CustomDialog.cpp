// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/QtUtils/CustomDialog.h"

#include <QPushButton>

CustomDialog::CustomDialog(QWidget *parent)
    : QDialog(parent)
{
}

void CustomDialog::setLayout(QLayout *layout)
{
    QPushButton* fake_button = new QPushButton(QStringLiteral(""));
    fake_button->setDefault(true);

    layout->addWidget(fake_button);
    QDialog::setLayout(layout);
    fake_button->close();
}
