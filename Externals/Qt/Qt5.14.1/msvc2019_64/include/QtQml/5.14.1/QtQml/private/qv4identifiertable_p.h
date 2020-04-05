/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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
#ifndef QV4IDENTIFIERTABLE_H
#define QV4IDENTIFIERTABLE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "qv4identifier_p.h"
#include "qv4string_p.h"
#include "qv4engine_p.h"
#include <qset.h>
#include <limits.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct Q_QML_PRIVATE_EXPORT IdentifierTable
{
    ExecutionEngine *engine;

    uint alloc;
    uint size;
    int numBits;
    Heap::StringOrSymbol **entriesByHash;
    Heap::StringOrSymbol **entriesById;

    QSet<IdentifierHashData *> idHashes;

    void addEntry(Heap::StringOrSymbol *str);

public:

    IdentifierTable(ExecutionEngine *engine, int numBits = 8);
    ~IdentifierTable();

    Heap::String *insertString(const QString &s);
    Heap::Symbol *insertSymbol(const QString &s);

    PropertyKey asPropertyKey(const Heap::String *str) {
        if (str->identifier.isValid())
            return str->identifier;
        return asPropertyKeyImpl(str);
    }
    PropertyKey asPropertyKey(const QV4::String *str) {
        return asPropertyKey(str->d());
    }

    PropertyKey asPropertyKey(const QString &s);
    PropertyKey asPropertyKey(const char *s, int len);

    PropertyKey asPropertyKeyImpl(const Heap::String *str);

    Heap::StringOrSymbol *resolveId(PropertyKey i) const;
    Heap::String *stringForId(PropertyKey i) const;
    Heap::Symbol *symbolForId(PropertyKey i) const;

    void markObjects(MarkStack *markStack);
    void sweep();

    void addIdentifierHash(IdentifierHashData *h) {
        idHashes.insert(h);
    }
    void removeIdentifierHash(IdentifierHashData *h) {
        idHashes.remove(h);
    }
};

}

QT_END_NAMESPACE

#endif
