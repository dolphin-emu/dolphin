#pragma once

#include <QSettings>

class DMEConfig
{
public:
  static DMEConfig& getInstance();
  DMEConfig(DMEConfig const&) = delete;
  void operator=(DMEConfig const&) = delete;

  int getWatcherUpdateTimerMs() const;
  int getFreezeTimerMs() const;
  int getScannerUpdateTimerMs() const;
  int getViewerUpdateTimerMs() const;

  int getViewerNbrBytesSeparator() const;

  void setWatcherUpdateTimerMs(const int watcherUpdateTimerMs);
  void setFreezeTimerMs(const int freezeTimerMs);
  void setScannerUpdateTimerMs(const int scannerUpdateTimerMs);
  void setViewerUpdateTimerMs(const int viewerUpdateTimerMs);

  void setViewerNbrBytesSeparator(const int viewerNbrBytesSeparator);

private:
  DMEConfig();
  ~DMEConfig();

  QSettings* m_settings;
};
