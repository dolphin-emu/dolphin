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
#ifndef QV4PERSISTENT_H
#define QV4PERSISTENT_H

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

#include "qv4value_p.h"
#include "qv4managed_p.h"

QT_BEGIN_NAMESPACE

namespace QV4 {

struct Q_QML_EXPORT PersistentValueStorage
{
    PersistentValueStorage(ExecutionEngine *engine);
    ~PersistentValueStorage();

    Value *allocate();
    static void free(Value *e);

    void mark(MarkStack *markStack);

    struct Iterator {
        Iterator(void *p, int idx);
        Iterator(const Iterator &o);
        Iterator & operator=(const Iterator &o);
        ~Iterator();
        void *p;
        int index;
        Iterator &operator++();
        bool operator !=(const Iterator &other) {
            return p != other.p || index != other.index;
        }
        Value &operator *();
    };
    Iterator begin() { return Iterator(firstPage, 0); }
    Iterator end() { return Iterator(nullptr, 0); }

    static ExecutionEngine *getEngine(Value *v);

    ExecutionEngine *engine;
    void *firstPage;
private:
    static void freePage(void *page);
};

class Q_QML_EXPORT PersistentValue
{
public:
    PersistentValue() {}
    PersistentValue(const PersistentValue &other);
    PersistentValue &operator=(const PersistentValue &other);
    PersistentValue &operator=(const WeakValue &other);
    PersistentValue &operator=(Object *object);
    ~PersistentValue();

    PersistentValue(ExecutionEngine *engine, const Value &value);
    PersistentValue(ExecutionEngine *engine, ReturnedValue value);
    PersistentValue(ExecutionEngine *engine, Object *object);

    void set(ExecutionEngine *engine, const Value &value);
    void set(ExecutionEngine *engine, ReturnedValue value);
    void set(ExecutionEngine *engine, Heap::Base *obj);

    ReturnedValue value() const {
        return (val ? val->asReturnedValue() : Encode::undefined());
    }
    Value *valueRef() const {
        return val;
    }
    Managed *asManaged() const {
        if (!val)
            return nullptr;
        return val->managed();
    }
    template<typename T>
    T *as() const {
        if (!val)
            return nullptr;
        return val->as<T>();
    }

    ExecutionEngine *engine() const {
        if (!val)
            return nullptr;
        return PersistentValueStorage::getEngine(val);
    }

    bool isUndefined() const { return !val || val->isUndefined(); }
    bool isNullOrUndefined() const { return !val || val->isNullOrUndefined(); }
    void clear() {
        PersistentValueStorage::free(val);
        val = nullptr;
    }
    bool isEmpty() { return !val; }

private:
    Value *val = nullptr;
};

class Q_QML_EXPORT WeakValue
{
public:
    WeakValue() {}
    WeakValue(const WeakValue &other);
    WeakValue(ExecutionEngine *engine, const Value &value);
    WeakValue &operator=(const WeakValue &other);
    ~WeakValue();

    void set(ExecutionEngine *engine, const Value &value)
    {
        if (!val)
            allocVal(engine);
        *val = value;
    }

    void set(ExecutionEngine *engine, ReturnedValue value)
    {
        if (!val)
            allocVal(engine);
        *val = value;
    }

    void set(ExecutionEngine *engine, Heap::Base *obj)
    {
        if (!val)
            allocVal(engine);
        *val = obj;
    }

    ReturnedValue value() const {
        return (val ? val->asReturnedValue() : Encode::undefined());
    }
    Value *valueRef() const {
        return val;
    }
    Managed *asManaged() const {
        if (!val)
            return nullptr;
        return val->managed();
    }
    template <typename T>
    T *as() const {
        if (!val)
            return nullptr;
        return val->as<T>();
    }

    ExecutionEngine *engine() const {
        if (!val)
            return nullptr;
        return PersistentValueStorage::getEngine(val);
    }

    bool isUndefined() const { return !val || val->isUndefined(); }
    bool isNullOrUndefined() const { return !val || val->isNullOrUndefined(); }
    void clear() { free(); }

    void markOnce(MarkStack *markStack);

private:
    Value *val = nullptr;

private:
    Q_NEVER_INLINE void allocVal(ExecutionEngine *engine);

    void free();
};

} // namespace QV4

QT_END_NAMESPACE

#endif
