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

#ifndef QQMLPROPERTYCACHE_P_H
#define QQMLPROPERTYCACHE_P_H

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

#include <private/qqmlrefcount_p.h>
#include <private/qflagpointer_p.h>
#include "qqmlcleanup_p.h"
#include "qqmlnotifier_p.h"
#include <private/qqmlpropertyindex_p.h>

#include <private/qlinkedstringhash_p.h>
#include <QtCore/qvarlengtharray.h>
#include <QtCore/qvector.h>

#include <private/qv4value_p.h>
#include <private/qqmlpropertydata_p.h>
#include <private/qqmlenumdata_p.h>
#include <private/qqmlenumvalue_p.h>

#include <limits>

QT_BEGIN_NAMESPACE

class QCryptographicHash;
class QJSEngine;
class QMetaObjectBuilder;
class QQmlVMEMetaObject;
class QQmlPropertyCacheMethodArguments;

class Q_QML_PRIVATE_EXPORT QQmlPropertyCache : public QQmlRefCount
{
public:
    QQmlPropertyCache();
    QQmlPropertyCache(const QMetaObject *, int metaObjectRevision = 0);
    ~QQmlPropertyCache() override;

    void update(const QMetaObject *);
    void invalidate(const QMetaObject *);

    QQmlPropertyCache *copy();

    QQmlPropertyCache *copyAndAppend(const QMetaObject *,
                QQmlPropertyData::Flags propertyFlags = QQmlPropertyData::Flags(),
                QQmlPropertyData::Flags methodFlags = QQmlPropertyData::Flags(),
                QQmlPropertyData::Flags signalFlags = QQmlPropertyData::Flags());
    QQmlPropertyCache *copyAndAppend(const QMetaObject *, int typeMinorVersion,
                QQmlPropertyData::Flags propertyFlags = QQmlPropertyData::Flags(),
                QQmlPropertyData::Flags methodFlags = QQmlPropertyData::Flags(),
                QQmlPropertyData::Flags signalFlags = QQmlPropertyData::Flags());

    QQmlPropertyCache *copyAndReserve(int propertyCount,
                                      int methodCount, int signalCount, int enumCount);
    void appendProperty(const QString &, QQmlPropertyData::Flags flags, int coreIndex,
                        int propType, int revision, int notifyIndex);
    void appendSignal(const QString &, QQmlPropertyData::Flags, int coreIndex,
                      const int *types = nullptr, const QList<QByteArray> &names = QList<QByteArray>());
    void appendMethod(const QString &, QQmlPropertyData::Flags flags, int coreIndex, int returnType,
                      const QList<QByteArray> &names, const QVector<int> &parameterTypes);
    void appendEnum(const QString &, const QVector<QQmlEnumValue> &);

    const QMetaObject *metaObject() const;
    const QMetaObject *createMetaObject();
    const QMetaObject *firstCppMetaObject() const;

    template<typename K>
    QQmlPropertyData *property(const K &key, QObject *object, QQmlContextData *context) const
    {
        return findProperty(stringCache.find(key), object, context);
    }

    QQmlPropertyData *property(int) const;
    QQmlPropertyData *maybeUnresolvedProperty(int) const;
    QQmlPropertyData *method(int) const;
    QQmlPropertyData *signal(int index) const;
    QQmlEnumData *qmlEnum(int) const;
    int methodIndexToSignalIndex(int) const;

    QString defaultPropertyName() const;
    QQmlPropertyData *defaultProperty() const;
    QQmlPropertyCache *parent() const;
    // is used by the Qml Designer
    void setParent(QQmlPropertyCache *newParent);

    inline QQmlPropertyData *overrideData(QQmlPropertyData *) const;
    inline bool isAllowedInRevision(QQmlPropertyData *) const;

    static QQmlPropertyData *property(QJSEngine *, QObject *, const QStringRef &,
                                              QQmlContextData *, QQmlPropertyData &);
    static QQmlPropertyData *property(QJSEngine *, QObject *, const QLatin1String &,
                                      QQmlContextData *, QQmlPropertyData &);
    static QQmlPropertyData *property(QJSEngine *, QObject *, const QV4::String *,
                                              QQmlContextData *, QQmlPropertyData &);

    static QQmlPropertyData *property(QJSEngine *engine, QObject *obj, const QString &name,
                                      QQmlContextData *context, QQmlPropertyData &local)
    {
        return property(engine, obj, QStringRef(&name), context, local);
    }

    //see QMetaObjectPrivate::originalClone
    int originalClone(int index);
    static int originalClone(QObject *, int index);

    QList<QByteArray> signalParameterNames(int index) const;
    static QString signalParameterStringForJS(QV4::ExecutionEngine *engine, const QList<QByteArray> &parameterNameList, QString *errorString = nullptr);

    const char *className() const;

    inline int propertyCount() const;
    inline int propertyOffset() const;
    inline int methodCount() const;
    inline int methodOffset() const;
    inline int signalCount() const;
    inline int signalOffset() const;
    inline int qmlEnumCount() const;

    static bool isDynamicMetaObject(const QMetaObject *);

    void toMetaObjectBuilder(QMetaObjectBuilder &);

    inline bool callJSFactoryMethod(QObject *object, void **args) const;

    static bool determineMetaObjectSizes(const QMetaObject &mo, int *fieldCount, int *stringCount);
    static bool addToHash(QCryptographicHash &hash, const QMetaObject &mo);

    QByteArray checksum(bool *ok);

    int allowedRevision(int index) const { return allowedRevisionCache[index]; }
    void setAllowedRevision(int index, int allowed) { allowedRevisionCache[index] = allowed; }

private:
    friend class QQmlEnginePrivate;
    friend class QQmlCompiler;
    template <typename T> friend class QQmlPropertyCacheCreator;
    template <typename T> friend class QQmlPropertyCacheAliasCreator;
    friend class QQmlComponentAndAliasResolver;
    friend class QQmlMetaObject;

    inline QQmlPropertyCache *copy(int reserve);

    void append(const QMetaObject *, int typeMinorVersion,
                QQmlPropertyData::Flags propertyFlags = QQmlPropertyData::Flags(),
                QQmlPropertyData::Flags methodFlags = QQmlPropertyData::Flags(),
                QQmlPropertyData::Flags signalFlags = QQmlPropertyData::Flags());

    QQmlPropertyCacheMethodArguments *createArgumentsObject(int count, const QList<QByteArray> &names);

    typedef QVector<QQmlPropertyData> IndexCache;
    typedef QLinkedStringMultiHash<QPair<int, QQmlPropertyData *> > StringCache;
    typedef QVector<int> AllowedRevisionCache;

    QQmlPropertyData *findProperty(StringCache::ConstIterator it, QObject *, QQmlContextData *) const;
    QQmlPropertyData *findProperty(StringCache::ConstIterator it, const QQmlVMEMetaObject *, QQmlContextData *) const;

    QQmlPropertyData *ensureResolved(QQmlPropertyData*) const;

    Q_NEVER_INLINE void resolve(QQmlPropertyData *) const;
    void updateRecur(const QMetaObject *);

    template<typename K>
    QQmlPropertyData *findNamedProperty(const K &key) const
    {
        StringCache::mapped_type *it = stringCache.value(key);
        return it ? it->second : 0;
    }

    template<typename K>
    void setNamedProperty(const K &key, int index, QQmlPropertyData *data, bool isOverride)
    {
        stringCache.insert(key, qMakePair(index, data));
        _hasPropertyOverrides |= isOverride;
    }

private:
    QQmlPropertyCache *_parent;
    int propertyIndexCacheStart;
    int methodIndexCacheStart;
    int signalHandlerIndexCacheStart;

    IndexCache propertyIndexCache;
    IndexCache methodIndexCache;
    IndexCache signalHandlerIndexCache;
    StringCache stringCache;
    AllowedRevisionCache allowedRevisionCache;
    QVector<QQmlEnumData> enumCache;

    bool _hasPropertyOverrides : 1;
    bool _ownMetaObject : 1;
    const QMetaObject *_metaObject;
    QByteArray _dynamicClassName;
    QByteArray _dynamicStringData;
    QString _defaultPropertyName;
    QQmlPropertyCacheMethodArguments *argumentsCache;
    int _jsFactoryMethodIndex;
    QByteArray _checksum;
};

inline QQmlPropertyData *QQmlPropertyCache::ensureResolved(QQmlPropertyData *p) const
{
    if (p && Q_UNLIKELY(p->notFullyResolved()))
        resolve(p);

    return p;
}

// Returns this property cache's metaObject.  May be null if it hasn't been created yet.
inline const QMetaObject *QQmlPropertyCache::metaObject() const
{
    return _metaObject;
}

// Returns the first C++ type's QMetaObject - that is, the first QMetaObject not created by
// QML
inline const QMetaObject *QQmlPropertyCache::firstCppMetaObject() const
{
    while (_parent && (_metaObject == nullptr || _ownMetaObject))
        return _parent->firstCppMetaObject();
    return _metaObject;
}

inline QQmlPropertyData *QQmlPropertyCache::property(int index) const
{
    if (index < 0 || index >= (propertyIndexCacheStart + propertyIndexCache.count()))
        return nullptr;

    if (index < propertyIndexCacheStart)
        return _parent->property(index);

    QQmlPropertyData *rv = const_cast<QQmlPropertyData *>(&propertyIndexCache.at(index - propertyIndexCacheStart));
    return ensureResolved(rv);
}

inline QQmlPropertyData *QQmlPropertyCache::method(int index) const
{
    if (index < 0 || index >= (methodIndexCacheStart + methodIndexCache.count()))
        return nullptr;

    if (index < methodIndexCacheStart)
        return _parent->method(index);

    QQmlPropertyData *rv = const_cast<QQmlPropertyData *>(&methodIndexCache.at(index - methodIndexCacheStart));
    return ensureResolved(rv);
}

/*! \internal
    \a index MUST be in the signal index range (see QObjectPrivate::signalIndex()).
    This is different from QMetaMethod::methodIndex().
*/
inline QQmlPropertyData *QQmlPropertyCache::signal(int index) const
{
    if (index < 0 || index >= (signalHandlerIndexCacheStart + signalHandlerIndexCache.count()))
        return nullptr;

    if (index < signalHandlerIndexCacheStart)
        return _parent->signal(index);

    QQmlPropertyData *rv = const_cast<QQmlPropertyData *>(&methodIndexCache.at(index - signalHandlerIndexCacheStart));
    Q_ASSERT(rv->isSignal() || rv->coreIndex() == -1);
    return ensureResolved(rv);
}

inline QQmlEnumData *QQmlPropertyCache::qmlEnum(int index) const
{
    if (index < 0 || index >= enumCache.count())
        return nullptr;

    return const_cast<QQmlEnumData *>(&enumCache.at(index));
}

inline int QQmlPropertyCache::methodIndexToSignalIndex(int index) const
{
    if (index < 0 || index >= (methodIndexCacheStart + methodIndexCache.count()))
        return index;

    if (index < methodIndexCacheStart)
        return _parent->methodIndexToSignalIndex(index);

    return index - methodIndexCacheStart + signalHandlerIndexCacheStart;
}

// Returns the name of the default property for this cache
inline QString QQmlPropertyCache::defaultPropertyName() const
{
    return _defaultPropertyName;
}

inline QQmlPropertyCache *QQmlPropertyCache::parent() const
{
    return _parent;
}

QQmlPropertyData *
QQmlPropertyCache::overrideData(QQmlPropertyData *data) const
{
    if (!data->hasOverride())
        return nullptr;

    if (data->overrideIndexIsProperty())
        return property(data->overrideIndex());
    else
        return method(data->overrideIndex());
}

bool QQmlPropertyCache::isAllowedInRevision(QQmlPropertyData *data) const
{
    return (data->metaObjectOffset() == -1 && data->revision() == 0) ||
           (allowedRevisionCache[data->metaObjectOffset()] >= data->revision());
}

int QQmlPropertyCache::propertyCount() const
{
    return propertyIndexCacheStart + propertyIndexCache.count();
}

int QQmlPropertyCache::propertyOffset() const
{
    return propertyIndexCacheStart;
}

int QQmlPropertyCache::methodCount() const
{
    return methodIndexCacheStart + methodIndexCache.count();
}

int QQmlPropertyCache::methodOffset() const
{
    return methodIndexCacheStart;
}

int QQmlPropertyCache::signalCount() const
{
    return signalHandlerIndexCacheStart + signalHandlerIndexCache.count();
}

int QQmlPropertyCache::signalOffset() const
{
    return signalHandlerIndexCacheStart;
}

int QQmlPropertyCache::qmlEnumCount() const
{
    return enumCache.count();
}

bool QQmlPropertyCache::callJSFactoryMethod(QObject *object, void **args) const
{
    if (_jsFactoryMethodIndex != -1) {
        _metaObject->d.static_metacall(object, QMetaObject::InvokeMetaMethod, _jsFactoryMethodIndex, args);
        return true;
    }
    if (_parent)
        return _parent->callJSFactoryMethod(object, args);
    return false;
}

QT_END_NAMESPACE

#endif // QQMLPROPERTYCACHE_P_H
