/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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

#ifndef QHSTS_P_H
#define QHSTS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the Network Access API.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include <QtNetwork/private/qtnetworkglobal_p.h>

#include <QtNetwork/qhstspolicy.h>

#include <QtCore/qbytearray.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qstring.h>
#include <QtCore/qglobal.h>
#include <QtCore/qpair.h>
#include <QtCore/qurl.h>

#include <map>

QT_BEGIN_NAMESPACE

template<typename T> class QList;
template <typename T> class QVector;

class Q_AUTOTEST_EXPORT QHstsCache
{
public:

    void updateFromHeaders(const QList<QPair<QByteArray, QByteArray>> &headers,
                           const QUrl &url);
    void updateFromPolicies(const QVector<QHstsPolicy> &hosts);
    void updateKnownHost(const QUrl &url, const QDateTime &expires,
                         bool includeSubDomains);
    bool isKnownHost(const QUrl &url) const;
    void clear();

    QVector<QHstsPolicy> policies() const;

#if QT_CONFIG(settings)
    void setStore(class QHstsStore *store);
#endif // QT_CONFIG(settings)

private:

    void updateKnownHost(const QString &hostName, const QDateTime &expires,
                         bool includeSubDomains);

    struct HostName
    {
        explicit HostName(const QString &n) : name(n) { }
        explicit HostName(const QStringRef &r) : fragment(r) { }

        bool operator < (const HostName &rhs) const
        {
            if (fragment.size()) {
                if (rhs.fragment.size())
                    return fragment < rhs.fragment;
                return fragment < QStringRef(&rhs.name);
            }

            if (rhs.fragment.size())
                return QStringRef(&name) < rhs.fragment;
            return name < rhs.name;
        }

        // We use 'name' for a HostName object contained in our dictionary;
        // we use 'fragment' only during lookup, when chopping the complete host
        // name, removing subdomain names (such HostName object is 'transient', it
        // must not outlive the original QString object.
        QString name;
        QStringRef fragment;
    };

    mutable std::map<HostName, QHstsPolicy> knownHosts;
#if QT_CONFIG(settings)
    QHstsStore *hstsStore = nullptr;
#endif // QT_CONFIG(settings)
};

class Q_AUTOTEST_EXPORT QHstsHeaderParser
{
public:

    bool parse(const QList<QPair<QByteArray, QByteArray>> &headers);

    QDateTime expirationDate() const { return expiry; }
    bool includeSubDomains() const { return subDomainsFound; }

private:

    bool parseSTSHeader();
    bool parseDirective();
    bool processDirective(const QByteArray &name, const QByteArray &value);
    bool nextToken();

    QByteArray header;
    QByteArray token;

    QDateTime expiry;
    int tokenPos = 0;
    bool maxAgeFound = false;
    qint64 maxAge = 0;
    bool subDomainsFound = false;
};

QT_END_NAMESPACE

#endif
