/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#ifndef QQMLTYPE_P_H
#define QQMLTYPE_P_H

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

#include <functional>

#include <private/qtqmlglobal_p.h>
#include <private/qqmlrefcount_p.h>

#include <QtQml/qqmlprivate.h>
#include <QtQml/qjsvalue.h>

#include <QtCore/qobject.h>

QT_BEGIN_NAMESPACE

class QHashedCStringRef;
class QQmlTypePrivate;
class QHashedString;
class QHashedStringRef;
class QQmlCustomParser;
class QQmlEnginePrivate;
class QQmlPropertyCache;

namespace QV4 {
struct String;
}
struct CompositeMetaTypeIds;

class Q_QML_PRIVATE_EXPORT QQmlType
{
public:
    QQmlType();
    QQmlType(const QQmlType &other);
    QQmlType(QQmlType &&other);
    QQmlType &operator =(const QQmlType &other);
    QQmlType &operator =(QQmlType &&other);
    explicit QQmlType(const QQmlTypePrivate *priv);
    ~QQmlType();

    bool operator ==(const QQmlType &other) const {
        return d.data() == other.d.data();
    }

    bool isValid() const { return !d.isNull(); }

    QByteArray typeName() const;
    QString qmlTypeName() const;
    QString elementName() const;

    QHashedString module() const;
    int majorVersion() const;
    int minorVersion() const;

    bool availableInVersion(int vmajor, int vminor) const;
    bool availableInVersion(const QHashedStringRef &module, int vmajor, int vminor) const;

    QObject *create() const;
    void create(QObject **, void **, size_t) const;

    typedef void (*CreateFunc)(void *);
    CreateFunc createFunction() const;
    QQmlCustomParser *customParser() const;

    bool isCreatable() const;
    typedef QObject *(*ExtensionFunc)(QObject *);
    ExtensionFunc extensionFunction() const;
    bool isExtendedType() const;
    QString noCreationReason() const;

    bool isSingleton() const;
    bool isInterface() const;
    bool isComposite() const;
    bool isCompositeSingleton() const;
    bool isQObjectSingleton() const;
    bool isQJSValueSingleton() const;

    int typeId() const;
    int qListTypeId() const;

    const QMetaObject *metaObject() const;
    const QMetaObject *baseMetaObject() const;
    int metaObjectRevision() const;
    bool containsRevisionedAttributes() const;

    QQmlAttachedPropertiesFunc attachedPropertiesFunction(QQmlEnginePrivate *engine) const;
    const QMetaObject *attachedPropertiesType(QQmlEnginePrivate *engine) const;
#if QT_DEPRECATED_SINCE(5, 14)
    QT_DEPRECATED int attachedPropertiesId(QQmlEnginePrivate *engine) const;
#endif

    int parserStatusCast() const;
    const char *interfaceIId() const;
    int propertyValueSourceCast() const;
    int propertyValueInterceptorCast() const;

    int index() const;

    bool isInlineComponentType() const;
    int inlineComponendId() const;

    struct Q_QML_PRIVATE_EXPORT SingletonInstanceInfo
    {
        QJSValue (*scriptCallback)(QQmlEngine *, QJSEngine *) = nullptr;
        std::function<QObject *(QQmlEngine *, QJSEngine *)> qobjectCallback = {};
        const QMetaObject *instanceMetaObject = nullptr;
        QString typeName;
        QUrl url; // used by composite singletons
    };
    SingletonInstanceInfo *singletonInstanceInfo() const;

    QUrl sourceUrl() const;

    int enumValue(QQmlEnginePrivate *engine, const QHashedStringRef &, bool *ok) const;
    int enumValue(QQmlEnginePrivate *engine, const QHashedCStringRef &, bool *ok) const;
    int enumValue(QQmlEnginePrivate *engine, const QV4::String *, bool *ok) const;

    int scopedEnumIndex(QQmlEnginePrivate *engine, const QV4::String *, bool *ok) const;
    int scopedEnumIndex(QQmlEnginePrivate *engine, const QString &, bool *ok) const;
    int scopedEnumValue(QQmlEnginePrivate *engine, int index, const QV4::String *, bool *ok) const;
    int scopedEnumValue(QQmlEnginePrivate *engine, int index, const QString &, bool *ok) const;
    int scopedEnumValue(QQmlEnginePrivate *engine, const QByteArray &, const QByteArray &, bool *ok) const;
    int scopedEnumValue(QQmlEnginePrivate *engine, const QStringRef &, const QStringRef &, bool *ok) const;
    int inlineComponentObjectId();
    void setInlineComponentObjectId(int id) const; // TODO: const setters are BAD

    const QQmlTypePrivate *priv() const { return d.data(); }
    static void refHandle(const QQmlTypePrivate *priv);
    static void derefHandle(const QQmlTypePrivate *priv);
    static int refCount(const QQmlTypePrivate *priv);

    enum RegistrationType {
        CppType = 0,
        SingletonType = 1,
        InterfaceType = 2,
        CompositeType = 3,
        CompositeSingletonType = 4,
        InlineComponentType = 5,
        AnyRegistrationType = 255
    };

    QQmlType containingType() const;
    int lookupInlineComponentIdByName(const QString &name) const;
    QQmlType lookupInlineComponentById(int objectid) const;
    int generatePlaceHolderICId() const;

    void associateInlineComponent(const QString &name, int objectID, const CompositeMetaTypeIds &metaTypeIds, QQmlType existingType);
    void setPendingResolutionName(const QString &name);
    QString pendingResolutionName() const;

private:
    friend class QQmlTypePrivate;
    friend uint qHash(const QQmlType &t, uint seed);
    QQmlRefPointer<const QQmlTypePrivate> d;
};

inline uint qHash(const QQmlType &t, uint seed = 0)
{
    return qHash(reinterpret_cast<quintptr>(t.d.data()), seed);
}

QT_END_NAMESPACE

#endif // QQMLTYPE_P_H
