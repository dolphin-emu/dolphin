#include <QtCore>
#include "CommonPaths.h"
#include "FileSearch.h"
#include "ConfigManager.h"
#include "ISOFile.h"
#include "CDUtils.h"
#include "GameList.h"


// TODO: A class for the QML to talk to. Flesh it out.
class InterQt : public QObject
{
    Q_OBJECT
public:
    InterQt(QObject* parent = 0);
    Q_INVOKABLE void refreshList();
    Q_INVOKABLE void startPauseEmu();
    Q_INVOKABLE void loadISO();
    Q_INVOKABLE void addISOFolder(QString path);
    Q_INVOKABLE void stopEmu();
    Q_INVOKABLE void switchFullscreen();
    Q_INVOKABLE void takeScreenshot();
    GameList games;
private:
    QList<QString>* pathList;
};

