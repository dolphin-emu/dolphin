/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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
#ifndef QQMLOBJECTCREATOR_P_H
#define QQMLOBJECTCREATOR_P_H

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

#include <private/qqmlimport_p.h>
#include <private/qqmltypenamecache_p.h>
#include <private/qv4compileddata_p.h>
#include <private/qfinitestack_p.h>
#include <private/qrecursionwatcher_p.h>
#include <private/qqmlprofiler_p.h>
#include <private/qv4qmlcontext_p.h>

#include <qpointer.h>

QT_BEGIN_NAMESPACE

class QQmlAbstractBinding;
class QQmlInstantiationInterrupt;
class QQmlIncubatorPrivate;

struct AliasToRequiredInfo {
    QString propertyName;
    QUrl fileUrl;
};

/*!
\internal
This struct contains information solely used for displaying error messages
\variable aliasesToRequired allows us to give the user a way to know which (aliasing) properties
can be set to set the required property
\sa QQmlComponentPrivate::unsetRequiredPropertyToQQmlError
*/
struct RequiredPropertyInfo
{
    QString propertyName;
    QUrl fileUrl;
    QV4::CompiledData::Location location;
    QVector<AliasToRequiredInfo> aliasesToRequired;
};

class RequiredProperties : public QHash<QQmlPropertyData*, RequiredPropertyInfo> {};

struct QQmlObjectCreatorSharedState : public QSharedData
{
    QQmlContextData *rootContext;
    QQmlContextData *creationContext;
    QFiniteStack<QQmlAbstractBinding::Ptr> allCreatedBindings;
    QFiniteStack<QQmlParserStatus*> allParserStatusCallbacks;
    QFiniteStack<QPointer<QObject> > allCreatedObjects;
    QV4::Value *allJavaScriptObjects; // pointer to vector on JS stack to reference JS wrappers during creation phase.
    QQmlComponentAttached *componentAttached;
    QList<QQmlEnginePrivate::FinalizeCallback> finalizeCallbacks;
    QQmlVmeProfiler profiler;
    QRecursionNode recursionNode;
    RequiredProperties requiredProperties;
    bool hadRequiredProperties;
};

class Q_QML_PRIVATE_EXPORT QQmlObjectCreator
{
    Q_DECLARE_TR_FUNCTIONS(QQmlObjectCreator)
public:
    QQmlObjectCreator(QQmlContextData *parentContext, const QQmlRefPointer<QV4::ExecutableCompilationUnit> &compilationUnit, QQmlContextData *creationContext, QQmlIncubatorPrivate  *incubator = nullptr);
    ~QQmlObjectCreator();

    enum CreationFlags { NormalObject = 1, InlineComponent = 2 };
    QObject *create(int subComponentIndex = -1, QObject *parent = nullptr, QQmlInstantiationInterrupt *interrupt = nullptr, int flags = NormalObject);

    bool populateDeferredProperties(QObject *instance, const QQmlData::DeferredData *deferredData);

    void beginPopulateDeferred(QQmlContextData *context);
    void populateDeferredBinding(const QQmlProperty &qmlProperty, int deferredIndex,
                                 const QV4::CompiledData::Binding *binding);
    void finalizePopulateDeferred();

    QQmlContextData *finalize(QQmlInstantiationInterrupt &interrupt);
    void clear();

    QQmlComponentAttached **componentAttachment() { return &sharedState->componentAttached; }

    QList<QQmlEnginePrivate::FinalizeCallback> *finalizeCallbacks() { return &sharedState->finalizeCallbacks; }

    QList<QQmlError> errors;

    QQmlContextData *parentContextData() const { return parentContext.contextData(); }
    QFiniteStack<QPointer<QObject> > &allCreatedObjects() { return sharedState->allCreatedObjects; }

    RequiredProperties &requiredProperties() {return sharedState->requiredProperties;}
    bool componentHadRequiredProperties() const {return sharedState->hadRequiredProperties;}

private:
    QQmlObjectCreator(QQmlContextData *contextData, const QQmlRefPointer<QV4::ExecutableCompilationUnit> &compilationUnit, QQmlObjectCreatorSharedState *inheritedSharedState);

    void init(QQmlContextData *parentContext);

    QObject *createInstance(int index, QObject *parent = nullptr, bool isContextObject = false);

    bool populateInstance(int index, QObject *instance,
                          QObject *bindingTarget, const QQmlPropertyData *valueTypeProperty);

    // If qmlProperty and binding are null, populate all properties, otherwise only the given one.
    void populateDeferred(QObject *instance, int deferredIndex,
                          const QQmlPropertyPrivate *qmlProperty = nullptr,
                          const QV4::CompiledData::Binding *binding = nullptr);

    void setupBindings(bool applyDeferredBindings = false);
    bool setPropertyBinding(const QQmlPropertyData *property, const QV4::CompiledData::Binding *binding);
    void setPropertyValue(const QQmlPropertyData *property, const QV4::CompiledData::Binding *binding);
    void setupFunctions();

    QString stringAt(int idx) const { return compilationUnit->stringAt(idx); }
    void recordError(const QV4::CompiledData::Location &location, const QString &description);

    void registerObjectWithContextById(const QV4::CompiledData::Object *object, QObject *instance) const;

    inline QV4::QmlContext *currentQmlContext();
    Q_NEVER_INLINE void createQmlContext();
    QV4::ResolvedTypeReference *resolvedType(int id) const
    {
        return compilationUnit->resolvedType(id);
    }

    enum Phase {
        Startup,
        CreatingObjects,
        CreatingObjectsPhase2,
        ObjectsCreated,
        Finalizing,
        Done
    } phase;

    QQmlEngine *engine;
    QV4::ExecutionEngine *v4;
    QQmlRefPointer<QV4::ExecutableCompilationUnit> compilationUnit;
    const QV4::CompiledData::Unit *qmlUnit;
    QQmlGuardedContextData parentContext;
    QQmlContextData *context;
    const QQmlPropertyCacheVector *propertyCaches;
    QExplicitlySharedDataPointer<QQmlObjectCreatorSharedState> sharedState;
    bool topLevelCreator;
    QQmlIncubatorPrivate *incubator;

    QObject *_qobject;
    QObject *_scopeObject;
    QObject *_bindingTarget;

    const QQmlPropertyData *_valueTypeProperty; // belongs to _qobjectForBindings's property cache
    int _compiledObjectIndex;
    const QV4::CompiledData::Object *_compiledObject;
    QQmlData *_ddata;
    QQmlRefPointer<QQmlPropertyCache> _propertyCache;
    QQmlVMEMetaObject *_vmeMetaObject;
    QQmlListProperty<void> _currentList;
    QV4::QmlContext *_qmlContext;

    friend struct QQmlObjectCreatorRecursionWatcher;

    typedef std::function<bool(QQmlObjectCreatorSharedState *sharedState)> PendingAliasBinding;
    std::vector<PendingAliasBinding> pendingAliasBindings;
};

struct QQmlObjectCreatorRecursionWatcher
{
    QQmlObjectCreatorRecursionWatcher(QQmlObjectCreator *creator);

    bool hasRecursed() const { return watcher.hasRecursed(); }

private:
    QExplicitlySharedDataPointer<QQmlObjectCreatorSharedState> sharedState;
    QRecursionWatcher<QQmlObjectCreatorSharedState, &QQmlObjectCreatorSharedState::recursionNode> watcher;
};

QV4::QmlContext *QQmlObjectCreator::currentQmlContext()
{
    if (!_qmlContext->isManaged())
        _qmlContext->setM(QV4::QmlContext::create(v4->rootContext(), context, _scopeObject));

    return _qmlContext;
}

QT_END_NAMESPACE

#endif // QQMLOBJECTCREATOR_P_H
