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

#ifndef QQMLMETATYPEDATA_P_H
#define QQMLMETATYPEDATA_P_H

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
#include <private/qqmlmetatype_p.h>
#include <private/qhashedstring_p.h>

#include <QtCore/qset.h>
#include <QtCore/qvector.h>
#include <QtCore/qbitarray.h>

QT_BEGIN_NAMESPACE

class QQmlTypePrivate;
struct QQmlMetaTypeData
{
    QQmlMetaTypeData();
    ~QQmlMetaTypeData();
    void registerType(QQmlTypePrivate *priv);
    QList<QQmlType> types;
    QSet<QQmlType> undeletableTypes;
    typedef QHash<int, QQmlTypePrivate *> Ids;
    Ids idToType;
    typedef QMultiHash<QHashedStringRef, QQmlTypePrivate *> Names;
    Names nameToType;
    typedef QHash<QUrl, QQmlTypePrivate *> Files; //For file imported composite types only
    Files urlToType;
    Files urlToNonFileImportType; // For non-file imported composite and composite
            // singleton types. This way we can locate any
            // of them by url, even if it was registered as
            // a module via QQmlPrivate::RegisterCompositeType
    typedef QMultiHash<const QMetaObject *, QQmlTypePrivate *> MetaObjects;
    MetaObjects metaObjectToType;
    typedef QHash<int, QQmlMetaType::StringConverter> StringConverters;
    StringConverters stringConverters;
    QVector<QHash<int, QQmlRefPointer<QQmlPropertyCache>>> typePropertyCaches;

    struct VersionedUri {
        VersionedUri()
            : majorVersion(0) {}
        VersionedUri(const QHashedString &uri, int majorVersion)
            : uri(uri), majorVersion(majorVersion) {}
        bool operator==(const VersionedUri &other) const {
            return other.majorVersion == majorVersion && other.uri == uri;
        }
        QHashedString uri;
        int majorVersion;
    };

    typedef QHash<VersionedUri, QQmlTypeModule *> TypeModules;
    TypeModules uriToModule;

    QHash<VersionedUri, void (*)()> moduleTypeRegistrationFunctions;
    bool registerModuleTypes(const VersionedUri &versionedUri);

    QBitArray objects;
    QBitArray interfaces;
    QBitArray lists;

    QList<QQmlPrivate::AutoParentFunction> parentFunctions;
    QVector<QQmlPrivate::QmlUnitCacheLookupFunction> lookupCachedQmlUnit;

    QHash<int, int> qmlLists;

    QHash<const QMetaObject *, QQmlPropertyCache *> propertyCaches;

    QQmlPropertyCache *propertyCacheForMinorVersion(int index, int minorVersion) const;
    void setPropertyCacheForMinorVersion(int index, int minorVersion, QQmlPropertyCache *cache);
    void clearPropertyCachesForMinorVersion(int index);

    QQmlPropertyCache *propertyCache(const QMetaObject *metaObject, int minorVersion);
    QQmlPropertyCache *propertyCache(const QQmlType &type, int minorVersion);

    void setTypeRegistrationFailures(QStringList *failures)
    {
        m_typeRegistrationFailures = failures;
    }

    void recordTypeRegFailure(const QString &message)
    {
        if (m_typeRegistrationFailures)
            m_typeRegistrationFailures->append(message);
        else
            qWarning("%s", message.toUtf8().constData());
    }

private:
    QStringList *m_typeRegistrationFailures = nullptr;
};

inline uint qHash(const QQmlMetaTypeData::VersionedUri &v)
{
    return v.uri.hash() ^ qHash(v.majorVersion);
}

QT_END_NAMESPACE

#endif // QQMLMETATYPEDATA_P_H
