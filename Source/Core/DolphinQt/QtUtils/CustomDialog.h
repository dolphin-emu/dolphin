// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QLayout>

// A custom QDialog that injects a fake default button when setting an input QLayout
class CustomDialog : public QDialog
{
public:
    explicit CustomDialog(QWidget *parent = 0);

    void setLayout(QLayout *layout);
};
