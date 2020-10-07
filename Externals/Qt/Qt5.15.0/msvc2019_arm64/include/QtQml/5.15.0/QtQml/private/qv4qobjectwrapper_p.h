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

#ifndef QV4QOBJECTWRAPPER_P_H
#define QV4QOBJECTWRAPPER_P_H

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

#include <QtCore/qglobal.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qpair.h>
#include <QtCore/qhash.h>
#include <private/qqmldata_p.h>
#include <private/qintrusivelist_p.h>

#include <private/qv4value_p.h>
#include <private/qv4functionobject_p.h>
#include <private/qv4lookup_p.h>

QT_BEGIN_NAMESPACE

class QObject;
class QQmlData;
class QQmlPropertyCache;
class QQmlPropertyData;

namespace QV4 {
struct QObjectSlotDispatcher;

namespace Heap {

struct QQmlValueTypeWrapper;

struct Q_QML_EXPORT QObjectWrapper : Object {
    void init(QObject *object)
    {
        Object::init();
        qObj.init(object);
    }

    void destroy() {
        qObj.destroy();
        Object::destroy();
    }

    QObject *object() const { return qObj.data(); }
    static void markObjects(Heap::Base *that, MarkStack *markStack);

private:
    QQmlQPointer<QObject> qObj;
};

#define QObjectMethodMembers(class, Member) \
    Member(class, Pointer, QQmlValueTypeWrapper *, valueTypeWrapper) \
    Member(class, NoMark, QQmlQPointer<QObject>, qObj) \
    Member(class, NoMark, QQmlPropertyCache *, _propertyCache) \
    Member(class, NoMark, int, index)

DECLARE_HEAP_OBJECT(QObjectMethod, FunctionObject) {
    DECLARE_MARKOBJECTS(QObjectMethod);

    void init(QV4::ExecutionContext *scope);
    void destroy()
    {
        setPropertyCache(nullptr);
        qObj.destroy();
        FunctionObject::destroy();
    }

    QQmlPropertyCache *propertyCache() const { return _propertyCache; }
    void setPropertyCache(QQmlPropertyCache *c) {
        if (c)
            c->addref();
        if (_propertyCache)
            _propertyCache->release();
        _propertyCache = c;
    }

    const QMetaObject *metaObject();
    QObject *object() const { return qObj.data(); }
    void setObject(QObject *o) { qObj = o; }

};

struct QMetaObjectWrapper : FunctionObject {
    const QMetaObject* metaObject;
    QQmlPropertyData *constructors;
    int constructorCount;

    void init(const QMetaObject* metaObject);
    void destroy();
    void ensureConstructorsCache();
};

struct QmlSignalHandler : Object {
    void init(QObject *object, int signalIndex);
    void destroy() {
        qObj.destroy();
        Object::destroy();
    }
    int signalIndex;

    QObject *object() const { return qObj.data(); }
    void setObject(QObject *o) { qObj = o; }

private:
    QQmlQPointer<QObject> qObj;
};

}

struct Q_QML_EXPORT QObjectWrapper : public Object
{
    V4_OBJECT2(QObjectWrapper, Object)
    V4_NEEDS_DESTROY

    enum RevisionMode { IgnoreRevision, CheckRevision };

    static void initializeBindings(ExecutionEngine *engine);

    QObject *object() const { return d()->object(); }

    ReturnedValue getQmlProperty(QQmlContextData *qmlContext, String *name, RevisionMode revisionMode, bool *hasProperty = nullptr, bool includeImports = false) const;
    static ReturnedValue getQmlProperty(ExecutionEngine *engine, QQmlContextData *qmlContext, QObject *object, String *name, RevisionMode revisionMode, bool *hasProperty = nullptr, QQmlPropertyData **property = nullptr);

    static bool setQmlProperty(ExecutionEngine *engine, QQmlContextData *qmlContext, QObject *object, String *name, RevisionMode revisionMode, const Value &value);

    static ReturnedValue wrap(ExecutionEngine *engine, QObject *object);
    static void markWrapper(QObject *object, MarkStack *markStack);

    using Object::get;

    static void setProperty(ExecutionEngine *engine, QObject *object, int propertyIndex, const Value &value);
    void setProperty(ExecutionEngine *engine, int propertyIndex, const Value &value);

    void destroyObject(bool lastCall);

    static ReturnedValue getProperty(ExecutionEngine *engine, QObject *object, QQmlPropertyData *property);

    static ReturnedValue virtualResolveLookupGetter(const Object *object, ExecutionEngine *engine, Lookup *lookup);
    static ReturnedValue lookupGetter(Lookup *l, ExecutionEngine *engine, const Value &object);
    template <typename ReversalFunctor> static ReturnedValue lookupGetterImpl(Lookup *l, ExecutionEngine *engine, const Value &object, bool useOriginalProperty, ReversalFunctor revert);
    static bool virtualResolveLookupSetter(Object *object, ExecutionEngine *engine, Lookup *lookup, const Value &value);

protected:
    static void setProperty(ExecutionEngine *engine, QObject *object, QQmlPropertyData *property, const Value &value);

    static bool virtualIsEqualTo(Managed *that, Managed *o);
    static ReturnedValue create(ExecutionEngine *engine, QObject *object);

    static QQmlPropertyData *findProperty(ExecutionEngine *engine, QObject *o, QQmlContextData *qmlContext, String *name, RevisionMode revisionMode, QQmlPropertyData *local);
    QQmlPropertyData *findProperty(ExecutionEngine *engine, QQmlContextData *qmlContext, String *name, RevisionMode revisionMode, QQmlPropertyData *local) const;

    static ReturnedValue virtualGet(const Managed *m, PropertyKey id, const Value *receiver, bool *hasProperty);
    static bool virtualPut(Managed *m, PropertyKey id, const Value &value, Value *receiver);
    static PropertyAttributes virtualGetOwnProperty(const Managed *m, PropertyKey id, Property *p);
    static OwnPropertyKeyIterator *virtualOwnPropertyKeys(const Object *m, Value *target);

    static ReturnedValue method_connect(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);
    static ReturnedValue method_disconnect(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

private:
    Q_NEVER_INLINE static ReturnedValue wrap_slowPath(ExecutionEngine *engine, QObject *object);
};

inline ReturnedValue QObjectWrapper::wrap(ExecutionEngine *engine, QObject *object)
{
    if (Q_UNLIKELY(QQmlData::wasDeleted(object)))
        return QV4::Encode::null();

    auto ddata = QQmlData::get(object);
    if (Q_LIKELY(ddata && ddata->jsEngineId == engine->m_engineId && !ddata->jsWrapper.isUndefined())) {
        // We own the JS object
        return ddata->jsWrapper.value();
    }

    return wrap_slowPath(engine, object);
}

template <typename ReversalFunctor>
inline ReturnedValue QObjectWrapper::lookupGetterImpl(Lookup *lookup, ExecutionEngine *engine, const Value &object, bool useOriginalProperty, ReversalFunctor revertLookup)
{
    // we can safely cast to a QV4::Object here. If object is something else,
    // the internal class won't match
    Heap::Object *o = static_cast<Heap::Object *>(object.heapObject());
    if (!o || o->internalClass != lookup->qobjectLookup.ic)
        return revertLookup();

    const Heap::QObjectWrapper *This = static_cast<const Heap::QObjectWrapper *>(o);
    QObject *qobj = This->object();
    if (QQmlData::wasDeleted(qobj))
        return QV4::Encode::undefined();

    QQmlData *ddata = QQmlData::get(qobj, /*create*/false);
    if (!ddata)
        return revertLookup();

    QQmlPropertyData *property = lookup->qobjectLookup.propertyData;
    if (ddata->propertyCache != lookup->qobjectLookup.propertyCache) {
        if (property->isOverridden() && (!useOriginalProperty || property->isFunction() || property->isSignalHandler()))
            return revertLookup();

        QQmlPropertyCache *fromMo = ddata->propertyCache;
        QQmlPropertyCache *toMo = lookup->qobjectLookup.propertyCache;
        bool canConvert = false;
        while (fromMo) {
            if (fromMo == toMo) {
                canConvert = true;
                break;
            }
            fromMo = fromMo->parent();
        }
        if (!canConvert)
            return revertLookup();
    }

    return getProperty(engine, qobj, property);
}

struct QQmlValueTypeWrapper;

struct Q_QML_EXPORT QObjectMethod : public QV4::FunctionObject
{
    V4_OBJECT2(QObjectMethod, QV4::FunctionObject)
    V4_NEEDS_DESTROY

    enum { DestroyMethod = -1, ToStringMethod = -2 };

    static ReturnedValue create(QV4::ExecutionContext *scope, QObject *object, int index);
    static ReturnedValue create(QV4::ExecutionContext *scope, Heap::QQmlValueTypeWrapper *valueType, int index);

    int methodIndex() const { return d()->index; }
    QObject *object() const { return d()->object(); }

    QV4::ReturnedValue method_toString(QV4::ExecutionEngine *engine) const;
    QV4::ReturnedValue method_destroy(QV4::ExecutionEngine *ctx, const Value *args, int argc) const;

    static ReturnedValue virtualCall(const FunctionObject *, const Value *thisObject, const Value *argv, int argc);

    ReturnedValue callInternal(const Value *thisObject, const Value *argv, int argc) const;

    static QPair<QObject *, int> extractQtMethod(const QV4::FunctionObject *function);
};


struct Q_QML_EXPORT QMetaObjectWrapper : public QV4::FunctionObject
{
    V4_OBJECT2(QMetaObjectWrapper, QV4::FunctionObject)
    V4_NEEDS_DESTROY

    static ReturnedValue create(ExecutionEngine *engine, const QMetaObject* metaObject);
    const QMetaObject *metaObject() const { return d()->metaObject; }

protected:
    static ReturnedValue virtualCallAsConstructor(const FunctionObject *, const Value *argv, int argc, const Value *);
    static bool virtualIsEqualTo(Managed *a, Managed *b);

private:
    void init(ExecutionEngine *engine);
    ReturnedValue constructInternal(const Value *argv, int argc) const;
    ReturnedValue callConstructor(const QQmlPropertyData &data, QV4::ExecutionEngine *engine, QV4::CallData *callArgs) const;
    ReturnedValue callOverloadedConstructor(QV4::ExecutionEngine *engine, QV4::CallData *callArgs) const;

};

struct Q_QML_EXPORT QmlSignalHandler : public QV4::Object
{
    V4_OBJECT2(QmlSignalHandler, QV4::Object)
    V4_PROTOTYPE(signalHandlerPrototype)
    V4_NEEDS_DESTROY

    int signalIndex() const { return d()->signalIndex; }
    QObject *object() const { return d()->object(); }

    static void initProto(ExecutionEngine *v4);
};

class MultiplyWrappedQObjectMap : public QObject,
                                  private QHash<QObject*, QV4::WeakValue>
{
    Q_OBJECT
public:
    typedef QHash<QObject*, QV4::WeakValue>::ConstIterator ConstIterator;
    typedef QHash<QObject*, QV4::WeakValue>::Iterator Iterator;

    ConstIterator begin() const { return QHash<QObject*, QV4::WeakValue>::constBegin(); }
    Iterator begin() { return QHash<QObject*, QV4::WeakValue>::begin(); }
    ConstIterator end() const { return QHash<QObject*, QV4::WeakValue>::constEnd(); }
    Iterator end() { return QHash<QObject*, QV4::WeakValue>::end(); }

    void insert(QObject *key, Heap::Object *value);
    ReturnedValue value(QObject *key) const
    {
        ConstIterator it = find(key);
        return it == end()
                ? QV4::WeakValue().value()
                : it->value();
    }

    Iterator erase(Iterator it);
    void remove(QObject *key);
    void mark(QObject *key, MarkStack *markStack);

private Q_SLOTS:
    void removeDestroyedObject(QObject*);
};

}

QT_END_NAMESPACE

#endif // QV4QOBJECTWRAPPER_P_H


