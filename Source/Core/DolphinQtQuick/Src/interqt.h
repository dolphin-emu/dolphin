#include <QtCore>
#include "CommonPaths.h"
#include "FileSearch.h"
#include "ConfigManager.h"
#include "ISOFile.h"
#include "CDUtils.h"
#include "GameList.h"


// TODO: A class for the QML to talk to. Flesh it out.
class InterQt
{
public:
    InterQt() {};
    Q_INVOKABLE void RefreshList();
    Q_INVOKABLE void StartPauseEmu();
    Q_INVOKABLE void LoadISO();
    Q_INVOKABLE void StopEmu();
    Q_INVOKABLE void SwitchFullscreen();
    Q_INVOKABLE void TakeScreenshot();
    GameList games;
};

