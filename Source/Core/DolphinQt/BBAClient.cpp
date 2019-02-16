// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/BBAClient.h"

#include <QDataStream>
#include <QHostAddress>
#include <QTcpSocket>

#include "DolphinQt/BBADebug.h"
#include "DolphinQt/BBAServer.h"

BBAClient::BBAClient(QTcpSocket& socket, BBAServer& server)
    : QObject(&server), m_socket(socket), m_server(server)
{
  m_socket.setParent(this);
  connect(&m_socket, &QIODevice::readyRead, this, &BBAClient::ReadyRead);
  connect(&m_socket, &QAbstractSocket::disconnected, this, [this]() {
    LogInfo() << m_socket.peerAddress() << m_socket.peerPort() << "disconnected";
    deleteLater();
  });

  LogInfo() << m_socket.peerAddress() << m_socket.peerPort() << "connected";
}

BBADebug BBAClient::LogInfo()
{
  return m_server.LogInfo();
}

void BBAClient::SendMessage(const QByteArray& buffer)
{
  {
    QDataStream data_stream(&m_socket);
    data_stream.setByteOrder(QDataStream::LittleEndian);
    data_stream << buffer.size();
  }
  m_socket.write(buffer);
}

void BBAClient::ReadyRead()
{
  m_buffer.append(m_socket.readAll());

  while (m_buffer.size())
  {
    switch (m_state)
    {
    case SocketState::Size:
      if (m_buffer.size() < sizeof(int))
        return;
      {
        QDataStream dataStream(&m_buffer, QIODevice::ReadOnly);
        dataStream.setByteOrder(QDataStream::LittleEndian);
        dataStream >> m_size;
      }
      m_buffer.remove(0, sizeof(int));
      m_state = SocketState::Payload;
      Q_FALLTHROUGH();
    case SocketState::Payload:
      if (m_buffer.size() < m_size)
        return;
      const auto payload = m_buffer.left(m_size);
      Q_EMIT ReceivedMessage(payload);
      m_buffer.remove(0, m_size);
      m_state = SocketState::Size;
    }
  }
}
