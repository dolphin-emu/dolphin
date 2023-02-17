#include "DMEConfig.h"

DMEConfig::DMEConfig()
{
  m_settings = new QSettings(QSettings::tr("settings.ini"), QSettings::IniFormat);
}

DMEConfig::~DMEConfig()
{
  delete m_settings;
}

DMEConfig& DMEConfig::getInstance()
{
  static DMEConfig instance;
  return instance;
}

int DMEConfig::getWatcherUpdateTimerMs() const
{
  return m_settings->value(QSettings::tr("timerSettings/watcherUpdateTimerMs"), 100).toInt();
}

int DMEConfig::getFreezeTimerMs() const
{
  return m_settings->value(QSettings::tr("timerSettings/freezeTimerMs"), 10).toInt();
}

int DMEConfig::getScannerUpdateTimerMs() const
{
  return m_settings->value(QSettings::tr("timerSettings/scannerUpdateTimerMs"), 10).toInt();
}

int DMEConfig::getViewerUpdateTimerMs() const
{
  return m_settings->value(QSettings::tr("timerSettings/viewerUpdateTimerMs"), 100).toInt();
}

int DMEConfig::getViewerNbrBytesSeparator() const
{
  return m_settings->value(QSettings::tr("viewerSettings/nbrBytesSeparator"), 1).toInt();
}

void DMEConfig::setWatcherUpdateTimerMs(const int updateTimerMs)
{
  m_settings->setValue(QSettings::tr("timerSettings/watcherUpdateTimerMs"), updateTimerMs);
}

void DMEConfig::setFreezeTimerMs(const int freezeTimerMs)
{
  m_settings->setValue(QSettings::tr("timerSettings/freezeTimerMs"), freezeTimerMs);
}

void DMEConfig::setScannerUpdateTimerMs(const int scannerUpdateTimerMs)
{
  m_settings->setValue(QSettings::tr("timerSettings/scannerUpdateTimerMs"), scannerUpdateTimerMs);
}

void DMEConfig::setViewerUpdateTimerMs(const int viewerUpdateTimerMs)
{
  m_settings->setValue(QSettings::tr("timerSettings/viewerUpdateTimerMs"), viewerUpdateTimerMs);
}

void DMEConfig::setViewerNbrBytesSeparator(const int viewerNbrBytesSeparator)
{
  m_settings->setValue(QSettings::tr("viewerSettings/nbrBytesSeparator"), viewerNbrBytesSeparator);
}
