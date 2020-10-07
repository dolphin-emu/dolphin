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

#ifndef QQMLTYPELOADERQMLDIRCONTENT_P_H
#define QQMLTYPELOADERQMLDIRCONTENT_P_H

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

#include <private/qqmldirparser_p.h>

QT_BEGIN_NAMESPACE

class QQmlError;
class QQmlTypeLoaderQmldirContent
{
private:
    friend class QQmlTypeLoader;

    void setContent(const QString &location, const QString &content);
    void setError(const QQmlError &);

public:
    QQmlTypeLoaderQmldirContent();
    QQmlTypeLoaderQmldirContent(const QQmlTypeLoaderQmldirContent &) = default;
    QQmlTypeLoaderQmldirContent &operator=(const QQmlTypeLoaderQmldirContent &) = default;

    bool hasContent() const { return m_hasContent; }
    bool hasError() const;
    QList<QQmlError> errors(const QString &uri) const;

    QString typeNamespace() const;

    QQmlDirComponents components() const;
    QQmlDirScripts scripts() const;
    QQmlDirPlugins plugins() const;
    QStringList imports() const;

    QString pluginLocation() const;

    bool designerSupported() const;

private:
    QQmlDirParser m_parser;
    QString m_location;
    bool m_hasContent = false;
};

QT_END_NAMESPACE

#endif // QQMLTYPELOADERQMLDIRCONTENT_P_H
