/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef HUFFMAN_P_H
#define HUFFMAN_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE

class QByteArray;

namespace HPack
{

struct CodeEntry
{
    quint32 byteValue;
    quint32 huffmanCode;
    quint32 bitLength;
};

class BitOStream;

quint64 huffman_encoded_bit_length(const QByteArray &inputData);
void huffman_encode_string(const QByteArray &inputData, BitOStream &outputStream);

// PrefixTable:
// Huffman codes with a small bit length
// fit into a table (these are 'terminal' symbols),
// codes with longer codes require additional
// tables, so several symbols will have the same index
// in a table - pointing into the next table.
// Every table has an 'indexLength' - that's
// how many bits can fit in table's indices +
// 'prefixLength' - how many bits were addressed
// by its 'parent' table(s).
// All PrefixTables are kept in 'prefixTables' array.
// PrefixTable itself does not have any entries,
// it just holds table's prefix/index + 'offset' -
// there table's data starts in an array of all
// possible entries ('tableData').

struct PrefixTable
{
    PrefixTable()
        : prefixLength(),
          indexLength(),
          offset()
    {
    }

    PrefixTable(quint32 prefix, quint32 index)
        : prefixLength(prefix),
          indexLength(index),
          offset()
    {
    }

    quint32 size()const
    {
        // Number of entries table contains:
        return 1 << indexLength;
    }

    quint32 prefixLength;
    quint32 indexLength;
    quint32 offset;
};

// Table entry is either a terminal entry (thus probably the code found)
// or points into another table ('nextTable' - index into
// 'prefixTables' array). If it's a terminal, 'nextTable' index
// refers to the same table.

struct PrefixTableEntry
{
    PrefixTableEntry()
        : bitLength(),
          nextTable(),
          byteValue()
    {
    }

    quint32 bitLength;
    quint32 nextTable;
    quint32 byteValue;
};

class BitIStream;

class HuffmanDecoder
{
public:
    enum class BitConstants
    {
        rootPrefix = 9,
        childPrefix = 6
    };

    HuffmanDecoder();

    bool decodeStream(BitIStream &inputStream, QByteArray &outputBuffer);

private:
    quint32 addTable(quint32 prefixLength, quint32 indexLength);
    PrefixTableEntry tableEntry(const PrefixTable &table, quint32 index);
    void setTableEntry(const PrefixTable &table, quint32 index, const PrefixTableEntry &entry);

    std::vector<PrefixTable> prefixTables;
    std::vector<PrefixTableEntry> tableData;
    quint32 minCodeLength;
};

bool huffman_decode_string(BitIStream &inputStream, QByteArray *outputBuffer);

} // namespace HPack

QT_END_NAMESPACE

#endif

