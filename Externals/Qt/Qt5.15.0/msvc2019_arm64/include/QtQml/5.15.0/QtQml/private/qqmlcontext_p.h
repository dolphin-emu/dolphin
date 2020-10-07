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

#ifndef QQMLCONTEXT_P_H
#define QQMLCONTEXT_P_H

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

#include "qqmlcontext.h"

#include "qqmldata_p.h"
#include "qqmltypenamecache_p.h"
#include "qqmlnotifier_p.h"
#include "qqmllist.h"

#include <QtCore/qhash.h>
#include <QtQml/qjsvalue.h>
#include <QtCore/qset.h>

#include <private/qobject_p.h>
#include <private/qflagpointer_p.h>
#include <private/qqmlguard_p.h>

#include <private/qv4executablecompilationunit_p.h>
#include <private/qv4identifier_p.h>

QT_BEGIN_NAMESPACE

class QQmlContext;
class QQmlExpression;
class QQmlEngine;
class QQmlExpression;
class QQmlExpressionPrivate;
class QQmlJavaScriptExpression;
class QQmlContextData;
class QQmlGuardedContextData;
class QQmlIncubatorPrivate;

class QQmlContextPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQmlContext)
public:
    QQmlContextPrivate();

    QQmlContextData *data;

    QList<QVariant> propertyValues;
    int notifyIndex;

    static QQmlContextPrivate *get(QQmlContext *context) {
        return static_cast<QQmlContextPrivate *>(QObjectPrivate::get(context));
    }
    static QQmlContext *get(QQmlContextPrivate *context) {
        return static_cast<QQmlContext *>(context->q_func());
    }

    // Only used for debugging
    QList<QPointer<QObject> > instances;

    static int context_count(QQmlListProperty<QObject> *);
    static QObject *context_at(QQmlListProperty<QObject> *, int);

    void dropDestroyedQObject(const QString &name, QObject *destroyed);
};

class QQmlComponentAttached;

class Q_QML_PRIVATE_EXPORT QQmlContextData
{
public:
    QQmlContextData();
    QQmlContextData(QQmlContext *);
    void emitDestruction();
    void clearContext();
    void clearContextRecursively();
    void invalidate();

    inline bool isValid() const {
        return engine && (!isInternal || !contextObject || !QObjectPrivate::get(contextObject)->wasDeleted);
    }

    // My parent context and engine
    QQmlContextData *parent = nullptr;
    QQmlEngine *engine;

    void setParent(QQmlContextData *, bool stronglyReferencedByParent = false);
    void refreshExpressions();

    void addObject(QQmlData *data);

    QUrl resolvedUrl(const QUrl &);

    // My containing QQmlContext.  If isInternal is true this owns publicContext.
    // If internal is false publicContext owns this.
    QQmlContext *asQQmlContext();
    QQmlContextPrivate *asQQmlContextPrivate();
    quint32 refCount = 0;
    quint32 isInternal:1;
    quint32 isJSContext:1;
    quint32 isPragmaLibraryContext:1;
    quint32 unresolvedNames:1; // True if expressions in this context failed to resolve a toplevel name
    quint32 hasEmittedDestruction:1;
    quint32 isRootObjectInCreation:1;
    quint32 stronglyReferencedByParent:1;
    quint32 hasExtraObject:1; // used in QQmlDelegateModelItem::dataForObject to find the corresponding QQmlDelegateModelItem of an object
    quint32 dummy:24;
    QQmlContext *publicContext;

    union {
        // The incubator that is constructing this context if any
        QQmlIncubatorPrivate *incubator;
        // a pointer to extra data, currently only used in QQmlDelegateModel
        QObject *extraObject;
    };

    // Compilation unit for contexts that belong to a compiled type.
    QQmlRefPointer<QV4::ExecutableCompilationUnit> typeCompilationUnit;

    // object index in CompiledData::Unit to component that created this context
    int componentObjectIndex;

    void initFromTypeCompilationUnit(const QQmlRefPointer<QV4::ExecutableCompilationUnit> &unit, int subComponentIndex);

    // flag indicates whether the context owns the cache (after mutation) or not.
    mutable QV4::IdentifierHash propertyNameCache;
    const QV4::IdentifierHash &propertyNames() const;
    QV4::IdentifierHash &detachedPropertyNames();

    // Context object
    QObject *contextObject;

    // Any script blocks that exist on this context
    QV4::PersistentValue importedScripts; // This is a JS Array

    QUrl baseUrl;
    QString baseUrlString;

    QUrl url() const;
    QString urlString() const;

    // List of imports that apply to this context
    QQmlRefPointer<QQmlTypeNameCache> imports;

    // My children
    QQmlContextData *childContexts = nullptr;

    // My peers in parent's childContexts list
    QQmlContextData  *nextChild;
    QQmlContextData **prevChild;

    // Expressions that use this context
    QQmlJavaScriptExpression *expressions;

    // Doubly-linked list of objects that are owned by this context
    QQmlData *contextObjects;

    // Doubly-linked list of context guards (XXX merge with contextObjects)
    QQmlGuardedContextData *contextGuards = nullptr;

    // id guards
    struct ContextGuard : public QQmlGuard<QObject>
    {
        inline ContextGuard();
        inline ContextGuard &operator=(QObject *obj);
        inline void objectDestroyed(QObject *) override;

        inline bool wasSet() const;

        QFlagPointer<QQmlContextData> context;
        QQmlNotifier bindings;
    };
    ContextGuard *idValues;
    int idValueCount;
    void setIdProperty(int, QObject *);

    // Linked contexts. this owns linkedContext.
    QQmlContextDataRef linkedContext;

    // Linked list of uses of the Component attached property in this
    // context
    QQmlComponentAttached *componentAttached;

    // Return the outermost id for obj, if any.
    QString findObjectId(const QObject *obj) const;

    static QQmlContextData *get(QQmlContext *context) {
        return QQmlContextPrivate::get(context)->data;
    }

private:
    friend class QQmlContextDataRef;
    friend class QQmlContext; // needs to do manual refcounting :/
    void refreshExpressionsRecursive(bool isGlobal);
    void refreshExpressionsRecursive(QQmlJavaScriptExpression *);
    ~QQmlContextData();
    void destroy();
};


class QQmlGuardedContextData
{
public:
    inline QQmlGuardedContextData() = default;
    inline QQmlGuardedContextData(QQmlContextData *data)
    { setContextData(data); }
    inline ~QQmlGuardedContextData()
    { clear(); }

    inline QQmlContextData *contextData() const
    { return m_contextData; }
    inline void setContextData(QQmlContextData *);

    inline bool isNull() const { return !m_contextData; }

    inline operator QQmlContextData*() const { return m_contextData; }
    inline QQmlContextData* operator->() const { return m_contextData; }
    inline QQmlGuardedContextData &operator=(QQmlContextData *d) {
        setContextData(d); return *this;
    }

private:
    QQmlGuardedContextData &operator=(const QQmlGuardedContextData &) = delete;
    QQmlGuardedContextData(const QQmlGuardedContextData &) = delete;
    friend class QQmlContextData;

    inline void clear();

    QQmlContextData *m_contextData = nullptr;
    QQmlGuardedContextData  *m_next = nullptr;
    QQmlGuardedContextData **m_prev = nullptr;
};


void QQmlGuardedContextData::setContextData(QQmlContextData *contextData)
 {
    if (m_contextData == contextData)
        return;
    clear();

    if (contextData) {
        m_contextData = contextData;
        m_next = contextData->contextGuards;
        if (m_next) m_next->m_prev = &m_next;
        m_prev = &contextData->contextGuards;
        contextData->contextGuards = this;
    }
}

void QQmlGuardedContextData::clear()
{
    if (m_prev) {
        *m_prev = m_next;
        if (m_next) m_next->m_prev = m_prev;
        m_contextData = nullptr;
        m_next = nullptr;
        m_prev = nullptr;
    }
}

QQmlContextDataRef::QQmlContextDataRef()
    : m_contextData(nullptr)
{
}

QQmlContextDataRef::QQmlContextDataRef(const QQmlContextDataRef &other)
    : m_contextData(other.m_contextData)
{
    if (m_contextData)
        ++m_contextData->refCount;
}

QQmlContextDataRef::QQmlContextDataRef(QQmlContextData *data)
    : m_contextData(data)
{
    if (m_contextData)
        ++m_contextData->refCount;
}

QQmlContextDataRef::~QQmlContextDataRef()
{
    clear();
}

void QQmlContextDataRef::setContextData(QQmlContextData *contextData)
{
    if (m_contextData == contextData)
        return;
    clear();

    if (contextData) {
        m_contextData = contextData;
        ++m_contextData->refCount;
    }
}

QQmlContextData *QQmlContextDataRef::contextData() const
{
    return m_contextData;
}

void QQmlContextDataRef::clear()
{
    if (m_contextData && !--m_contextData->refCount)
        m_contextData->destroy();
    m_contextData = nullptr;
}

QQmlContextDataRef &
QQmlContextDataRef::operator=(QQmlContextData *d)
{
    setContextData(d);
    return *this;
}

QQmlContextDataRef &
QQmlContextDataRef::operator=(const QQmlContextDataRef &other)
{
    setContextData(other.m_contextData);
    return *this;
}

QQmlContextData::ContextGuard::ContextGuard()
: context(nullptr)
{
}

QQmlContextData::ContextGuard &QQmlContextData::ContextGuard::operator=(QObject *obj)
{
    QQmlGuard<QObject>::operator=(obj);
    context.setFlag();
    bindings.notify(); // For alias connections
    return *this;
}

void QQmlContextData::ContextGuard::objectDestroyed(QObject *)
{
    if (context->contextObject && !QObjectPrivate::get(context->contextObject)->wasDeleted)
        bindings.notify();
}

bool QQmlContextData::ContextGuard::wasSet() const
{
    return context.flag();
}

QT_END_NAMESPACE

#endif // QQMLCONTEXT_P_H
