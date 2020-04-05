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

#ifndef QQMLTYPENAMECACHE_P_H
#define QQMLTYPENAMECACHE_P_H

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
#include "qqmlcleanup_p.h"
#include "qqmlmetatype_p.h"

#include <private/qstringhash_p.h>
#include <private/qqmlimport_p.h>
#include <private/qqmltypemoduleversion_p.h>

#include <QtCore/qvector.h>

QT_BEGIN_NAMESPACE

struct QQmlImportRef {
    inline QQmlImportRef()
        : scriptIndex(-1)
    {}
    // Imported module
    QVector<QQmlTypeModuleVersion> modules;

    // Or, imported script
    int scriptIndex;

    // Or, imported compositeSingletons
    QStringHash<QUrl> compositeSingletons;

    // The qualifier of this import
    QString m_qualifier;
};

class QQmlType;
class QQmlEngine;
class Q_QML_PRIVATE_EXPORT QQmlTypeNameCache : public QQmlRefCount
{
public:
    QQmlTypeNameCache(const QQmlImports &imports);
    ~QQmlTypeNameCache() override;

    inline bool isEmpty() const;

    void add(const QHashedString &name, int sciptIndex = -1, const QHashedString &nameSpace = QHashedString());
    void add(const QHashedString &name, const QUrl &url, const QHashedString &nameSpace = QHashedString());

    struct Result {
        inline Result();
        inline Result(const QQmlImportRef *importNamespace);
        inline Result(const QQmlType &type);
        inline Result(int scriptIndex);
        inline Result(const Result &);

        inline bool isValid() const;

        QQmlType type;
        const QQmlImportRef *importNamespace;
        int scriptIndex;
    };
    Result query(const QHashedStringRef &) const;
    Result query(const QHashedStringRef &, const QQmlImportRef *importNamespace) const;
    Result query(const QV4::String *, QQmlImport::RecursionRestriction recursionRestriction = QQmlImport::PreventRecursion) const;
    Result query(const QV4::String *, const QQmlImportRef *importNamespace) const;

private:
    friend class QQmlImports;

    template<typename Key>
    Result query(const QStringHash<QQmlImportRef> &imports, Key key) const
    {
        QQmlImportRef *i = imports.value(key);
        if (i) {
            Q_ASSERT(!i->m_qualifier.isEmpty());
            if (i->scriptIndex != -1) {
                return Result(i->scriptIndex);
            } else {
                return Result(i);
            }
        }

        return Result();
    }

    template<typename Key>
    Result query(const QStringHash<QUrl> &urls, Key key) const
    {
        QUrl *url = urls.value(key);
        if (url) {
            QQmlType type = QQmlMetaType::qmlType(*url);
            return Result(type);
        }

        return Result();
    }

    template<typename Key>
    Result typeSearch(const QVector<QQmlTypeModuleVersion> &modules, Key key) const
    {
        QVector<QQmlTypeModuleVersion>::const_iterator end = modules.constEnd();
        for (QVector<QQmlTypeModuleVersion>::const_iterator it = modules.constBegin(); it != end; ++it) {
            QQmlType type = it->type(key);
            if (type.isValid())
                return Result(type);
        }

        return Result();
    }

    QStringHash<QQmlImportRef> m_namedImports;
    QMap<const QQmlImportRef *, QStringHash<QQmlImportRef> > m_namespacedImports;
    QVector<QQmlTypeModuleVersion> m_anonymousImports;
    QStringHash<QUrl> m_anonymousCompositeSingletons;
    QQmlImports m_imports;
};

QQmlTypeNameCache::Result::Result()
: importNamespace(nullptr), scriptIndex(-1)
{
}

QQmlTypeNameCache::Result::Result(const QQmlImportRef *importNamespace)
: importNamespace(importNamespace), scriptIndex(-1)
{
}

QQmlTypeNameCache::Result::Result(const QQmlType &type)
: type(type), importNamespace(nullptr), scriptIndex(-1)
{
}

QQmlTypeNameCache::Result::Result(int scriptIndex)
: importNamespace(nullptr), scriptIndex(scriptIndex)
{
}

QQmlTypeNameCache::Result::Result(const Result &o)
: type(o.type), importNamespace(o.importNamespace), scriptIndex(o.scriptIndex)
{
}

bool QQmlTypeNameCache::Result::isValid() const
{
    return type.isValid() || importNamespace || scriptIndex != -1;
}

bool QQmlTypeNameCache::isEmpty() const
{
    return m_namedImports.isEmpty() && m_anonymousImports.isEmpty()
        && m_anonymousCompositeSingletons.isEmpty();
}

QT_END_NAMESPACE

#endif // QQMLTYPENAMECACHE_P_H

