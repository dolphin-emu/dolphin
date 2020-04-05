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
#ifndef QV4JSONOBJECT_H
#define QV4JSONOBJECT_H

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

#include "qv4object_p.h"
#include <qjsonarray.h>
#include <qjsonobject.h>
#include <qjsonvalue.h>
#include <qjsondocument.h>
#include <qhash.h>

QT_BEGIN_NAMESPACE

namespace QV4 {

namespace Heap {

struct JsonObject : Object {
    void init();
};

}

struct ObjectItem {
    const QV4::Object *o;
    ObjectItem(const QV4::Object *o) : o(o) {}
};

inline bool operator ==(const ObjectItem &a, const ObjectItem &b)
{ return a.o->d() == b.o->d(); }

inline int qHash(const ObjectItem &i, uint seed = 0)
{ return ::qHash((void *)i.o->d(), seed); }

struct JsonObject : Object {
    Q_MANAGED_TYPE(JsonObject)
    V4_OBJECT2(JsonObject, Object)
private:

    typedef QSet<ObjectItem> V4ObjectSet;
public:

    static ReturnedValue method_parse(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_stringify(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

    static ReturnedValue fromJsonValue(ExecutionEngine *engine, const QJsonValue &value);
    static ReturnedValue fromJsonObject(ExecutionEngine *engine, const QJsonObject &object);
    static ReturnedValue fromJsonArray(ExecutionEngine *engine, const QJsonArray &array);

    static inline QJsonValue toJsonValue(const QV4::Value &value)
    { V4ObjectSet visitedObjects; return toJsonValue(value, visitedObjects); }
    static inline QJsonObject toJsonObject(const QV4::Object *o)
    { V4ObjectSet visitedObjects; return toJsonObject(o, visitedObjects); }
    static inline QJsonArray toJsonArray(const QV4::ArrayObject *a)
    { V4ObjectSet visitedObjects; return toJsonArray(a, visitedObjects); }

private:
    static QJsonValue toJsonValue(const QV4::Value &value, V4ObjectSet &visitedObjects);
    static QJsonObject toJsonObject(const Object *o, V4ObjectSet &visitedObjects);
    static QJsonArray toJsonArray(const ArrayObject *a, V4ObjectSet &visitedObjects);

};

class JsonParser
{
public:
    JsonParser(ExecutionEngine *engine, const QChar *json, int length);

    ReturnedValue parse(QJsonParseError *error);

private:
    inline bool eatSpace();
    inline QChar nextToken();

    ReturnedValue parseObject();
    ReturnedValue parseArray();
    bool parseMember(Object *o);
    bool parseString(QString *string);
    bool parseValue(Value *val);
    bool parseNumber(Value *val);

    ExecutionEngine *engine;
    const QChar *head;
    const QChar *json;
    const QChar *end;

    int nestingLevel;
    QJsonParseError::ParseError lastError;
};

}

QT_END_NAMESPACE

#endif

