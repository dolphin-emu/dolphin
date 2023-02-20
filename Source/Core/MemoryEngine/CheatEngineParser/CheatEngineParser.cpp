#include "CheatEngineParser.h"

#include <iomanip>
#include <sstream>
#include <string>

#include "DolphinQt/MemoryEngine/GUICommon.h"
#include "../Common/MemoryCommon.h"

CheatEngineParser::CheatEngineParser()
{
  m_xmlReader = new QXmlStreamReader();
}

CheatEngineParser::~CheatEngineParser()
{
  delete m_xmlReader;
}

QString CheatEngineParser::getErrorMessages() const
{
  return m_errorMessages;
}

bool CheatEngineParser::hasACriticalErrorOccured() const
{
  return m_criticalErrorOccured;
}

void CheatEngineParser::setTableStartAddress(const u64 tableStartAddress)
{
  m_tableStartAddress = tableStartAddress;
}

MemWatchTreeNode* CheatEngineParser::parseCTFile(QIODevice* CTFileIODevice,
                                                 const bool useDolphinPointer)
{
  m_xmlReader->setDevice(CTFileIODevice);

  if (m_xmlReader->readNextStartElement())
  {
    std::string test = m_xmlReader->name().toString().toStdString();
    if (m_xmlReader->name() == QString("CheatTable"))
    {
      MemWatchTreeNode* rootNode = new MemWatchTreeNode(nullptr);
      parseCheatTable(rootNode, useDolphinPointer);
      if (m_xmlReader->hasError())
      {
        m_errorMessages = m_xmlReader->errorString();
        m_criticalErrorOccured = true;
        return nullptr;
      }
      else if (!m_errorMessages.isEmpty())
      {
        if (m_errorMessages.endsWith("\n\n"))
          m_errorMessages.remove(m_errorMessages.length() - 2, 2);
      }
      return rootNode;
    }
  }

  m_errorMessages = "The file provided is not a valid CT file";
  m_criticalErrorOccured = true;
  return nullptr;
}

MemWatchTreeNode* CheatEngineParser::parseCheatTable(MemWatchTreeNode* rootNode,
                                                     const bool useDolphinPointer)
{
  while (!(m_xmlReader->isEndElement() && m_xmlReader->name() == QString("CheatTable")))
  {
    if (m_xmlReader->readNextStartElement())
    {
      std::string test = m_xmlReader->name().toString().toStdString();
      if (m_xmlReader->name() == QString("CheatEntries"))
        parseCheatEntries(rootNode, useDolphinPointer);
    }
    if (m_xmlReader->hasError())
    {
      m_criticalErrorOccured = true;
      break;
    }
  }

  if (m_criticalErrorOccured)
    return nullptr;

  return rootNode;
}

MemWatchTreeNode* CheatEngineParser::parseCheatEntries(MemWatchTreeNode* node,
                                                       const bool useDolphinPointer)
{
  while (!(m_xmlReader->isEndElement() && m_xmlReader->name() == QString("CheatEntries")))
  {
    if (m_xmlReader->readNextStartElement())
    {
      std::string test = m_xmlReader->name().toString().toStdString();
      if (m_xmlReader->name() == QString("CheatEntry"))
        parseCheatEntry(node, useDolphinPointer);
    }
    if (m_xmlReader->hasError())
    {
      m_criticalErrorOccured = true;
      break;
    }
  }

  if (m_criticalErrorOccured)
    return nullptr;

  return node;
}

void CheatEngineParser::parseCheatEntry(MemWatchTreeNode* node, const bool useDolphinPointer)
{
  std::string label = "No label";
  u32 consoleAddress = (size_t)Common::getMEM1();
  Common::MemType type = Common::MemType::type_byte;
  Common::MemBase base = Common::MemBase::base_decimal;
  bool isUnsigned = true;
  size_t length = 1;
  bool isGroup = false;

  cheatEntryParsingState currentCheatEntryState;
  currentCheatEntryState.lineNumber = m_xmlReader->lineNumber();

  while (!(m_xmlReader->isEndElement() && m_xmlReader->name() == QString("CheatEntry")))
  {
    if (m_xmlReader->readNextStartElement())
    {
      if (m_xmlReader->name() == QString("Description"))
      {
        currentCheatEntryState.labelFound = true;
        label = m_xmlReader->readElementText().toStdString();
        if (label.at(0) == '"')
          label.erase(0, 1);
        if (label.at(label.length() - 1) == '"')
          label.erase(label.length() - 1, 1);
      }
      else if (m_xmlReader->name() == QString("VariableType"))
      {
        std::string strVarType = m_xmlReader->readElementText().toStdString();
        if (strVarType == "Custom")
        {
          continue;
        }
        else
        {
          currentCheatEntryState.typeFound = true;
          if (strVarType == "Byte" || strVarType == "Binary")
            type = Common::MemType::type_byte;
          else if (strVarType == "String")
            type = Common::MemType::type_string;
          else if (strVarType == "Array of byte")
            type = Common::MemType::type_byteArray;
          else
            currentCheatEntryState.validType = false;
        }
      }
      else if (m_xmlReader->name() == QString("CustomType"))
      {
        currentCheatEntryState.typeFound = true;
        std::string strVarType = m_xmlReader->readElementText().toStdString();
        if (strVarType == "2 Byte Big Endian")
          type = Common::MemType::type_halfword;
        else if (strVarType == "4 Byte Big Endian")
          type = Common::MemType::type_word;
        else if (strVarType == "Float Big Endian")
          type = Common::MemType::type_float;
        else
          currentCheatEntryState.validType = false;
      }
      else if (m_xmlReader->name() == QString("Length") ||
               m_xmlReader->name() == QString("ByteLength"))
      {
        currentCheatEntryState.lengthForStrFound = true;
        std::string strLength = m_xmlReader->readElementText().toStdString();
        std::stringstream ss(strLength);
        size_t lengthCandiate = 0;
        ss >> lengthCandiate;
        if (ss.fail() || lengthCandiate == 0)
          currentCheatEntryState.validLengthForStr = false;
        else
          length = lengthCandiate;
      }
      else if (m_xmlReader->name() == QString("Address"))
      {
        if (useDolphinPointer)
        {
          m_xmlReader->skipCurrentElement();
        }
        else
        {
          currentCheatEntryState.consoleAddressFound = true;
          u64 consoleAddressCandidate = 0;
          std::string strCEAddress = m_xmlReader->readElementText().toStdString();
          std::stringstream ss(strCEAddress);
          ss >> std::hex;
          ss >> consoleAddressCandidate;
          if (ss.fail())
          {
            currentCheatEntryState.validConsoleAddressHex = false;
          }
          else
          {
            consoleAddressCandidate -= m_tableStartAddress;
            consoleAddressCandidate += (size_t)Common::getMEM1();
            if (Common::isValidAddress(consoleAddressCandidate))
              consoleAddress = consoleAddressCandidate;
            else
              currentCheatEntryState.validConsoleAddress = false;
          }
        }
      }
      else if (m_xmlReader->name() == QString("Offsets"))
      {
        if (useDolphinPointer)
        {
          if (m_xmlReader->readNextStartElement())
          {
            if (m_xmlReader->name() == QString("Offset"))
            {
              currentCheatEntryState.consoleAddressFound = true;
              u32 consoleAddressCandidate = 0;
              std::string strOffset = m_xmlReader->readElementText().toStdString();
              std::stringstream ss(strOffset);
              ss >> std::hex;
              ss >> consoleAddressCandidate;
              if (ss.fail())
              {
                currentCheatEntryState.validConsoleAddressHex = false;
              }
              else
              {
                consoleAddressCandidate += (size_t)Common::getMEM1();
                if (Common::isValidAddress(consoleAddressCandidate))
                  consoleAddress = consoleAddressCandidate;
                else
                  currentCheatEntryState.validConsoleAddress = false;
              }
            }
          }
        }
        else
        {
          m_xmlReader->skipCurrentElement();
        }
      }
      else if (m_xmlReader->name() == QString("ShowAsHex"))
      {
        if (m_xmlReader->readElementText().toStdString() == "1")
          base = Common::MemBase::base_hexadecimal;
      }
      else if (m_xmlReader->name() == QString("ShowAsBinary"))
      {
        if (m_xmlReader->readElementText().toStdString() == "1")
          base = Common::MemBase::base_binary;
      }
      else if (m_xmlReader->name() == QString("ShowAsSigned"))
      {
        if (m_xmlReader->readElementText().toStdString() == "1")
          isUnsigned = false;
      }
      else if (m_xmlReader->name() == QString("CheatEntries"))
      {
        isGroup = true;
        MemWatchTreeNode* newNode =
            new MemWatchTreeNode(nullptr, node, true, QString::fromStdString(label));
        node->appendChild(newNode);
        parseCheatEntries(newNode, useDolphinPointer);
      }
      else
      {
        m_xmlReader->skipCurrentElement();
      }
    }
    if (m_xmlReader->hasError())
    {
      m_criticalErrorOccured = true;
      break;
    }
  }

  if (m_criticalErrorOccured)
    return;

  if (!isGroup)
  {
    MemWatchEntry* entry = new MemWatchEntry(QString::fromStdString(label), consoleAddress, type,
                                             base, isUnsigned, length, false);
    MemWatchTreeNode* newNode = new MemWatchTreeNode(entry, node, false, "");
    node->appendChild(newNode);
    verifyCheatEntryParsingErrors(currentCheatEntryState, entry, false, useDolphinPointer);
  }
  else
  {
    verifyCheatEntryParsingErrors(currentCheatEntryState, nullptr, true, useDolphinPointer);
  }
}

void CheatEngineParser::verifyCheatEntryParsingErrors(cheatEntryParsingState state,
                                                      MemWatchEntry* entry, bool isGroup,
                                                      const bool useDolphinPointer)
{
  QString stateErrors = "";
  QString consoleAddressFieldImport =
      useDolphinPointer ? "Dolphin pointer offset" : "Cheat Engine address";

  if (!state.labelFound)
    stateErrors += "No description was found\n";

  if (isGroup)
  {
    if (!stateErrors.isEmpty())
    {
      stateErrors = "No description was found while importing the Cheat Entry located at line " +
                    QString::number(state.lineNumber) +
                    " of the CT file as group, the label \"No label\" was imported instead\n\n";
      m_errorMessages += stateErrors;
    }
  }
  else
  {
    if (!state.typeFound)
    {
      stateErrors += "No type was found\n";
    }
    else if (!state.validType)
    {
      stateErrors += "A type was found, but was invalid\n";
    }
    else if (entry->getType() == Common::MemType::type_string)
    {
      if (!state.lengthForStrFound)
        stateErrors += "No length for the String type was found\n";
      else if (!state.validLengthForStr)
        stateErrors += "A length for the String type was found, but was invalid\n";
    }

    if (!state.consoleAddressFound)
      stateErrors += "No " + consoleAddressFieldImport + " was found\n";
    else if (!state.validConsoleAddressHex)
      stateErrors +=
          "An " + consoleAddressFieldImport + " was found, but was not a valid hexadcimal number\n";
    else if (!state.validConsoleAddress)
      stateErrors += "A valid " + consoleAddressFieldImport +
                     " was found, but lead to an invalid console address\n";

    if (!stateErrors.isEmpty())
    {
      stateErrors.prepend(
          "The following error(s) occured while importing the Cheat Entry located at line " +
          QString::number(state.lineNumber) + " of the CT file as watch entry:\n\n");
      stateErrors +=
          "\nThe following informations were imported instead to accomodate for this/these "
          "error(s):\n\n";
      stateErrors += formatImportedEntryBasicInfo(entry);
      stateErrors += "\n\n";
      m_errorMessages += stateErrors;
    }
  }
}

QString CheatEngineParser::formatImportedEntryBasicInfo(const MemWatchEntry* entry) const
{
  QString formatedEntry = "";
  formatedEntry += "Label: " + entry->getLabel() + "\n";
  formatedEntry +=
      "Type: " + GUICommon::getStringFromType(entry->getType(), entry->getLength()) + "\n";
  std::stringstream ss;
  ss << std::hex << std::uppercase << entry->getConsoleAddress();
  formatedEntry += "Address: " + QString::fromStdString(ss.str());
  return formatedEntry;
}
