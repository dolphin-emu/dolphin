#pragma once

#include <QComboBox>
#include <QDialog>
#include <QSpinBox>

#include <QDialogButtonBox>

class DlgSettings : public QDialog
{
public:
  DlgSettings(QWidget* parent = nullptr);
  ~DlgSettings();

private:
  void loadSettings();
  void saveSettings() const;

  QSpinBox* m_spnWatcherUpdateTimerMs;
  QSpinBox* m_spnScannerUpdateTimerMs;
  QSpinBox* m_spnViewerUpdateTimerMs;
  QSpinBox* m_spnFreezeTimerMs;
  QComboBox* m_cmbViewerBytesSeparator;
  QDialogButtonBox* m_buttonsDlg;
};
