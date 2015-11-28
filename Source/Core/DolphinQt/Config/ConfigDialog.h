// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QMainWindow>

#include "Core/ConfigManager.h"

// Predefinitions
class QAbstractButton;
namespace Ui
{
class DConfigDialog;
}

class DConfigDialog final : public QMainWindow
{
	Q_OBJECT

public:
	explicit DConfigDialog(QWidget* parent_widget = nullptr);
	~DConfigDialog();

protected:
	void eventFilter(QObject* watched, QEvent* event);

private:
	std::unique_ptr<Ui::DConfigDialog> m_ui;
	SConfig& m_conf;

	void UpdateIcons();
	void InitStaticData();
	void SetupSlots();
	void LoadSettings();

	void UpdateCpuOCLabel();
	void ChooseSlotPath(int device);
	void AudioBackendChanged();
};
