// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>
#include <QSet>

class QHostAddress;
class QNetworkProxy;
class QTcpServer;

class BBAClient;
class BBADebug;

class BBAServer : public QObject
{
  Q_OBJECT

public:
  explicit BBAServer(QObject* parent = nullptr);

  bool Listen(const QHostAddress& address, quint16 port = 0);
  bool Listen(const QString& address, quint16 port = 0);
  void Close();

  bool IsListening() const;

  void SetMaxPendingConnections(int num_connections);
  int MaxPendingConnections() const;

  quint16 ServerPort() const;
  QHostAddress ServerAddress() const;

  QString ErrorString() const;

  void PauseAccepting();
  void ResumeAccepting();

#ifndef QT_NO_NETWORKPROXY
  void SetProxy(const QNetworkProxy& network_proxy);
  QNetworkProxy Proxy() const;
#endif

  BBADebug LogInfo();

signals:
  void Information(const QDateTime& timestamp, const QString& message);

protected:
  void timerEvent(QTimerEvent* ev) override;

private slots:
  void NewConnection();

private:
  QSet<BBAClient*> m_clients;
  QTcpServer& m_server;

  int m_counter = 0;
  int m_timer_id = -1;
};
