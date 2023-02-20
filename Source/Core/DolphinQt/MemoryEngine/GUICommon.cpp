#include "GUICommon.h"

#include <QCoreApplication>
#include <QStringList>

namespace GUICommon
{
QStringList g_memTypeNames =
    QStringList({QCoreApplication::translate("Common", "Byte"),
                 QCoreApplication::translate("Common", "2 bytes (Halfword)"),
                 QCoreApplication::translate("Common", "4 bytes (Word)"),
                 QCoreApplication::translate("Common", "Float"),
                 QCoreApplication::translate("Common", "Double"),
                 QCoreApplication::translate("Common", "String"),
                 QCoreApplication::translate("Common", "Array of bytes")});

QStringList g_memBaseNames = QStringList({QCoreApplication::translate("Common", "Decimal"),
                                          QCoreApplication::translate("Common", "Hexadecimal"),
                                          QCoreApplication::translate("Common", "Octal"),
                                          QCoreApplication::translate("Common", "Binary")});

QStringList g_memScanFilter =
    QStringList({QCoreApplication::translate("Common", "Exact value"),
                 QCoreApplication::translate("Common", "Increased by"),
                 QCoreApplication::translate("Common", "Decreased by"),
                 QCoreApplication::translate("Common", "Between"),
                 QCoreApplication::translate("Common", "Bigger than"),
                 QCoreApplication::translate("Common", "Smaller than"),
                 QCoreApplication::translate("Common", "Increased"),
                 QCoreApplication::translate("Common", "Decreased"),
                 QCoreApplication::translate("Common", "Changed"),
                 QCoreApplication::translate("Common", "Unchanged"),
                 QCoreApplication::translate("Common", "Unknown initial value")});

bool g_valueEditing = false;

QString getStringFromType(const Common::MemType type, const size_t length)
{
  switch (type)
  {
  case Common::MemType::type_byte:
  case Common::MemType::type_halfword:
  case Common::MemType::type_word:
  case Common::MemType::type_float:
  case Common::MemType::type_double:
    return GUICommon::g_memTypeNames.at(static_cast<int>(type));
  case Common::MemType::type_string:
    return QString::fromStdString("string[" + std::to_string(length) + "]");
  case Common::MemType::type_byteArray:
    return QString::fromStdString("array of bytes[" + std::to_string(length) + "]");
  default:
    return QString();
  }
}

QString getNameFromBase(const Common::MemBase base)
{
  switch (base)
  {
  case Common::MemBase::base_binary:
    return QCoreApplication::translate("Common", "binary");
  case Common::MemBase::base_octal:
    return QCoreApplication::translate("Common", "octal");
  case Common::MemBase::base_decimal:
    return QCoreApplication::translate("Common", "decimal");
  case Common::MemBase::base_hexadecimal:
    return QCoreApplication::translate("Common", "hexadecimal");
  default:
    return QString();
  }
}
}  // namespace GUICommon
