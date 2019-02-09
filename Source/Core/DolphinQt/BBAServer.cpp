// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/BBAServer.h"

#include <QTcpServer>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QTimerEvent>

#include "DolphinQt/BBAClient.h"
#include "DolphinQt/BBADebug.h"

BBAServer::BBAServer(QObject *parent) :
  QObject(parent),
  m_server(*new QTcpServer(this))
{
  connect(&m_server, &QTcpServer::newConnection, this, &BBAServer::NewConnection);
}

bool BBAServer::Listen(const QHostAddress &address, quint16 port)
{
  const auto result = m_server.listen(address, port);
  if (result)
    LogInfo() << "Started listening on" << address << port;
  else
    LogInfo() << "Could not start listening on" << address << port << "because" << m_server.errorString();
  return result;
}

bool BBAServer::Listen(const QString &address, quint16 port)
{
  return Listen(address.isEmpty() ? QHostAddress::Any : QHostAddress(address), port);
}

void BBAServer::Close()
{
  LogInfo() << "Stopped listening";
  return m_server.close();
}

bool BBAServer::IsListening() const
{
  return m_server.isListening();
}

void BBAServer::SetMaxPendingConnections(int num_connections)
{
  m_server.setMaxPendingConnections(num_connections);
}

int BBAServer::MaxPendingConnections() const
{
  return m_server.maxPendingConnections();
}

quint16 BBAServer::ServerPort() const
{
  return m_server.serverPort();
}

QHostAddress BBAServer::ServerAddress() const
{
  return m_server.serverAddress();
}

QString BBAServer::ErrorString() const
{
  return m_server.errorString();
}

void BBAServer::PauseAccepting()
{
  m_server.pauseAccepting();
}

void BBAServer::ResumeAccepting()
{
  m_server.resumeAccepting();
}

#ifndef QT_NO_NETWORKPROXY
void BBAServer::SetProxy(const QNetworkProxy &network_proxy)
{
  m_server.setProxy(network_proxy);
}

QNetworkProxy BBAServer::Proxy() const
{
  return m_server.proxy();
}
#endif

BBADebug BBAServer::LogInfo()
{
  return BBADebug(*this);
}

void BBAServer::timerEvent(QTimerEvent *ev)
{
  if (ev->timerId() == m_timer_id)
  {
    if (m_counter <= 0)
        return;

    LogInfo() << m_counter << "packets transmitted";
    m_counter = 0;
  }
  else
  {
    QObject::timerEvent(ev);
  }
}

void BBAServer::NewConnection()
{
  auto* const socket = m_server.nextPendingConnection();
  if (!socket)
    return;

  auto* client = new BBAClient(*socket, *this);
  for(auto* other_client : m_clients)
  {
    connect(client, &BBAClient::ReceivedMessage, other_client, &BBAClient::SendMessage);
    connect(other_client, &BBAClient::ReceivedMessage, client, &BBAClient::SendMessage);
  }
  if (m_timer_id == -1)
    m_timer_id = startTimer(1000);
  connect(client, &BBAClient::ReceivedMessage, this, [&counter=m_counter](){ counter++; });
  connect(client, &QObject::destroyed, this, [client,this](){
    m_clients.remove(client);
    if (m_clients.empty() && m_timer_id != -1)
      killTimer(m_timer_id);
      m_timer_id = -1;
  });
  m_clients.insert(client);
}
