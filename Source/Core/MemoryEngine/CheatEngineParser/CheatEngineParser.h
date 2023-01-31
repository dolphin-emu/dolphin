#pragma once

#include <QFile>
#include <QString>
#include <QXmlStreamReader>

#include "../MemoryWatch/MemWatchTreeNode.h"

class CheatEngineParser
{
public:
  CheatEngineParser();
  ~CheatEngineParser();

  QString getErrorMessages() const;
  bool hasACriticalErrorOccured() const;
  void setTableStartAddress(const u64 tableStartAddress);
  MemWatchTreeNode* parseCTFile(QIODevice* CTFileIODevice, const bool useDolphinPointer);

private:
  struct cheatEntryParsingState
  {
    qint64 lineNumber = 0;

    bool labelFound = false;
    bool consoleAddressFound = false;
    bool typeFound = false;
    bool lengthForStrFound = false;

    bool validConsoleAddressHex = true;
    bool validConsoleAddress = true;
    bool validType = true;
    bool validLengthForStr = true;
  };

  MemWatchTreeNode* parseCheatTable(MemWatchTreeNode* rootNode, const bool useDolphinPointer);
  MemWatchTreeNode* parseCheatEntries(MemWatchTreeNode* node, const bool useDolphinPointer);
  void parseCheatEntry(MemWatchTreeNode* node, const bool useDolphinPointer);
  void verifyCheatEntryParsingErrors(cheatEntryParsingState state, MemWatchEntry* entry,
                                     bool isGroup, const bool useDolphinPointer);
  QString formatImportedEntryBasicInfo(const MemWatchEntry* entry) const;

  u64 m_tableStartAddress = 0;
  QString m_errorMessages = QObject::tr("");
  bool m_criticalErrorOccured = false;
  QXmlStreamReader* m_xmlReader;
};
