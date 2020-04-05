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
#ifndef QV4IDENTIFIER_H
#define QV4IDENTIFIER_H

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

#include <qstring.h>
#include <private/qv4global_p.h>
#include <private/qv4propertykey_p.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

struct IdentifierHashEntry {
    PropertyKey identifier;
    int value;
};

struct IdentifierHashData
{
    IdentifierHashData(IdentifierTable *table, int numBits);
    explicit IdentifierHashData(IdentifierHashData *other);
    ~IdentifierHashData();
    void markObjects(MarkStack *markStack) const;

    QBasicAtomicInt refCount;
    int alloc;
    int size;
    int numBits;
    IdentifierTable *identifierTable;
    IdentifierHashEntry *entries;
};

struct IdentifierHash
{

    IdentifierHashData *d = nullptr;

    IdentifierHash() {}
    IdentifierHash(ExecutionEngine *engine);
    inline IdentifierHash(const IdentifierHash &other);
    inline ~IdentifierHash();
    inline IdentifierHash &operator=(const IdentifierHash &other);

    bool isEmpty() const { return !d; }

    inline int count() const;

    void detach();

    void add(const QString &str, int value);
    void add(Heap::String *str, int value);

    inline int value(const QString &str) const;
    inline int value(String *str) const;
    QString findId(int value) const;

protected:
    IdentifierHashEntry *addEntry(PropertyKey i);
    const IdentifierHashEntry *lookup(PropertyKey identifier) const;
    const IdentifierHashEntry *lookup(const QString &str) const;
    const IdentifierHashEntry *lookup(String *str) const;
    const PropertyKey toIdentifier(const QString &str) const;
    const PropertyKey toIdentifier(Heap::String *str) const;
};


inline IdentifierHash::IdentifierHash(const IdentifierHash &other)
{
    d = other.d;
    if (d)
        d->refCount.ref();
}

inline IdentifierHash::~IdentifierHash()
{
    if (d && !d->refCount.deref())
        delete d;
}

IdentifierHash &IdentifierHash::operator=(const IdentifierHash &other)
{
    if (other.d)
        other.d->refCount.ref();
    if (d && !d->refCount.deref())
        delete d;
    d = other.d;
    return *this;
}

inline int IdentifierHash::count() const
{
    return d ? d->size : 0;
}

inline
void IdentifierHash::add(const QString &str, int value)
{
    IdentifierHashEntry *e = addEntry(toIdentifier(str));
    e->value = value;
}

inline
void IdentifierHash::add(Heap::String *str, int value)
{
    IdentifierHashEntry *e = addEntry(toIdentifier(str));
    e->value = value;
}

inline int IdentifierHash::value(const QString &str) const
{
    const IdentifierHashEntry *e = lookup(str);
    return e ? e->value : -1;
}

inline int IdentifierHash::value(String *str) const
{
    const IdentifierHashEntry *e = lookup(str);
    return e ? e->value : -1;
}

}

QT_END_NAMESPACE

#endif
