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

#ifndef QQMLTYPE_P_P_H
#define QQMLTYPE_P_P_H

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

#include <private/qqmltype_p.h>
#include <private/qstringhash_p.h>
#include <private/qqmlproxymetaobject_p.h>
#include <private/qqmlrefcount_p.h>
#include <private/qqmlpropertycache_p.h>
#include <private/qqmlmetatype_p.h>

QT_BEGIN_NAMESPACE

class QQmlTypePrivate : public QQmlRefCount
{
    Q_DISABLE_COPY_MOVE(QQmlTypePrivate)
public:
    QQmlTypePrivate(QQmlType::RegistrationType type);

    void init() const;
    void initEnums(QQmlEnginePrivate *engine) const;
    void insertEnums(const QMetaObject *metaObject) const;
    void insertEnumsFromPropertyCache(const QQmlPropertyCache *cache) const;
    void setContainingType(QQmlType *containingType);

    QUrl sourceUrl() const
    {
        switch (regType) {
        case QQmlType::CompositeType:
            return extraData.fd->url;
        case QQmlType::CompositeSingletonType:
            return extraData.sd->singletonInstanceInfo->url;
        case QQmlType::InlineComponentType:
            return extraData.id->url;
        default:
            return QUrl();
        }
    }

    const QQmlTypePrivate *attachedPropertiesBase(QQmlEnginePrivate *engine) const
    {
        for (const QQmlTypePrivate *d = this; d; d = d->resolveCompositeBaseType(engine).d.data()) {
            if (d->regType == QQmlType::CppType)
                return d->extraData.cd->attachedPropertiesType ? d : nullptr;

            if (d->regType != QQmlType::CompositeType)
                return nullptr;
        }
        return nullptr;
    }

    bool isComposite() const
    {
        return regType == QQmlType::CompositeType || regType == QQmlType::CompositeSingletonType;
    }

    QQmlType resolveCompositeBaseType(QQmlEnginePrivate *engine) const;
    QQmlPropertyCache *compositePropertyCache(QQmlEnginePrivate *engine) const;

    QQmlType::RegistrationType regType;

    struct QQmlCppTypeData
    {
        int allocationSize;
        void (*newFunc)(void *);
        QString noCreationReason;
        int parserStatusCast;
        QObject *(*extFunc)(QObject *);
        const QMetaObject *extMetaObject;
        QQmlCustomParser *customParser;
        QQmlAttachedPropertiesFunc attachedPropertiesFunc;
        const QMetaObject *attachedPropertiesType;
        int propertyValueSourceCast;
        int propertyValueInterceptorCast;
        bool registerEnumClassesUnscoped;
    };

    struct QQmlSingletonTypeData
    {
        QQmlType::SingletonInstanceInfo *singletonInstanceInfo;
    };

    struct QQmlCompositeTypeData
    {
        QUrl url;
    };

    struct QQmlInlineTypeData
    {
        QUrl url = QUrl();
        // The containing type stores a pointer to the inline component type
        // Using QQmlType here would create a reference cycle
        // As the inline component type cannot outlive the containing type
        // this should still be fine
        QQmlTypePrivate const * containingType = nullptr;
        QString inlineComponentName = QString();
        int objectId = -1;
    };

    union extraData {
        QQmlCppTypeData* cd;
        QQmlSingletonTypeData* sd;
        QQmlCompositeTypeData* fd;
        QQmlInlineTypeData* id;
    } extraData;

    const char *iid;
    QHashedString module;
    QString name;
    QString elementName;
    int version_maj;
    int version_min;
    int typeId;
    int listId;
    int revision;
    mutable bool containsRevisionedAttributes;
    mutable QQmlType superType;
    const QMetaObject *baseMetaObject;

    int index;
    mutable volatile bool isSetup:1;
    mutable volatile bool isEnumFromCacheSetup:1;
    mutable volatile bool isEnumFromBaseSetup:1;
    mutable bool haveSuperType:1;
    mutable QList<QQmlProxyMetaObject::ProxyData> metaObjects;
    mutable QStringHash<int> enums;
    mutable QStringHash<int> scopedEnumIndex; // maps from enum name to index in scopedEnums
    mutable QList<QStringHash<int>*> scopedEnums;

    void setName(const QString &uri, const QString &element);
    mutable QHash<QString, int> namesToInlineComponentObjectIndex;
    mutable QHash<int, QQmlType> objectIdToICType;

private:
    ~QQmlTypePrivate() override;

    struct EnumInfo {
        QStringList path;
        QString metaObjectName;
        QString enumName;
        QString enumKey;
        QString metaEnumScope;
        bool scoped;
    };

    void createListOfPossibleConflictingItems(const QMetaObject *metaObject, QList<EnumInfo> &enumInfoList, QStringList path) const;
    void createEnumConflictReport(const QMetaObject *metaObject, const QString &conflictingKey) const;
};

QT_END_NAMESPACE

#endif // QQMLTYPE_P_P_H
