// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStackedWidget>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

class SettingsWindow final : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsWindow(QWidget* parent = nullptr);

public slots:
    void changePage(QListWidgetItem* current, QListWidgetItem* previous);

private:
    void MakeCategoryList();
    void MakeUnfinishedWarning();
    void AddCategoryToList(const QString& title, const QString& icon);
    void SetupSettingsWidget();
    QStackedWidget* m_settings_outer;
    QListWidget* m_categories;
    QGroupBox* m_warning_group;
};
