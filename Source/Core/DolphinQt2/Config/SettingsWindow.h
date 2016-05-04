// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>

class SettingsWindow final : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsWindow(QWidget* parent = nullptr);

public slots:
    void changePage(QListWidgetItem *current, QListWidgetItem *previous);
signals:

private:
    void MakeCategoryList();
    void SetupSettingsWidget();
    QStackedWidget* m_settings_outer;
    QListWidget* m_categories;
};
