#include "SConfig.h"

SConfig::SConfig()
{
  m_settings = new QSettings(QSettings::tr("settings.ini"), QSettings::IniFormat);
}

SConfig::~SConfig()
{
  delete m_settings;
}

SConfig& SConfig::getInstance()
{
  static SConfig instance;
  return instance;
}

int SConfig::getWatcherUpdateTimerMs() const
{
  return m_settings->value(QSettings::tr("timerSettings/watcherUpdateTimerMs"), 100).toInt();
}

int SConfig::getFreezeTimerMs() const
{
  return m_settings->value(QSettings::tr("timerSettings/freezeTimerMs"), 10).toInt();
}

int SConfig::getScannerUpdateTimerMs() const
{
  return m_settings->value(QSettings::tr("timerSettings/scannerUpdateTimerMs"), 10).toInt();
}

int SConfig::getViewerUpdateTimerMs() const
{
  return m_settings->value(QSettings::tr("timerSettings/viewerUpdateTimerMs"), 100).toInt();
}

int SConfig::getViewerNbrBytesSeparator() const
{
  return m_settings->value(QSettings::tr("viewerSettings/nbrBytesSeparator"), 1).toInt();
}

void SConfig::setWatcherUpdateTimerMs(const int updateTimerMs)
{
  m_settings->setValue(QSettings::tr("timerSettings/watcherUpdateTimerMs"), updateTimerMs);
}

void SConfig::setFreezeTimerMs(const int freezeTimerMs)
{
  m_settings->setValue(QSettings::tr("timerSettings/freezeTimerMs"), freezeTimerMs);
}

void SConfig::setScannerUpdateTimerMs(const int scannerUpdateTimerMs)
{
  m_settings->setValue(QSettings::tr("timerSettings/scannerUpdateTimerMs"), scannerUpdateTimerMs);
}

void SConfig::setViewerUpdateTimerMs(const int viewerUpdateTimerMs)
{
  m_settings->setValue(QSettings::tr("timerSettings/viewerUpdateTimerMs"), viewerUpdateTimerMs);
}

void SConfig::setViewerNbrBytesSeparator(const int viewerNbrBytesSeparator)
{
  m_settings->setValue(QSettings::tr("viewerSettings/nbrBytesSeparator"), viewerNbrBytesSeparator);
}
