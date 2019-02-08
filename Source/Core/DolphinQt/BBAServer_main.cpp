#include <QCoreApplication>
#include <QDebug>

#include "DolphinQt/BBAServer.h"

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  BBAServer server(&app);

  QObject::connect(&server, &BBAServer::Information, [](const QDateTime &timestamp, const QString &logLine){
    Q_UNUSED(timestamp)
    qInfo().noquote() << logLine;
  });

  if(!server.Listen(QString(), 50000))
    return -1;

  return app.exec();
}
