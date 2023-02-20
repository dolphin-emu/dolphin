#pragma once

#include <QString>
#include <QStringList>

#include "../../MemoryEngine/Common/MemoryCommon.h"

namespace GUICommon
{
extern QStringList g_memTypeNames;
extern QStringList g_memScanFilter;
extern QStringList g_memBaseNames;

QString getStringFromType(const Common::MemType type, const size_t length = 0);
QString getNameFromBase(const Common::MemBase base);

extern bool g_valueEditing;
}  // namespace GUICommon
