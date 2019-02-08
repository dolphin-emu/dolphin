// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>

class QTcpSocket;

class BBAServer;
class BBADebug;

class BBAClient : public QObject
{
  Q_OBJECT

public:
  explicit BBAClient(QTcpSocket &socket, BBAServer &server);

  BBADebug LogInfo();

signals:
  void ReceivedMessage(const QByteArray &buffer);

public slots:
  void SendMessage(const QByteArray &buffer);

private slots:
  void ReadyRead();

private:
  enum class SocketState {
      Size,
      Payload
  };

  QTcpSocket &m_socket;
  BBAServer &m_server;

  QByteArray m_buffer;
  SocketState m_state;
  int m_size;
};
