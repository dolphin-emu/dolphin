// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

class QDialog;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;
class QString;
class QWidget;

class SettingsWindow final : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsWindow(QWidget* parent = nullptr);

public slots:
    void changePage(QListWidgetItem* current, QListWidgetItem* previous);

private:
    void MakeCategoryList();
    void AddCategoryToList(QString title, QString icon);
    void SetupSettingsWidget();
    QStackedWidget* m_settings_outer;
    QListWidget* m_categories;
};
