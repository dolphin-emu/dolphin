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

#ifndef QV4INCLUDE_P_H
#define QV4INCLUDE_P_H

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

#include <QtCore/qobject.h>
#include <QtCore/qurl.h>

#include <private/qqmlcontext_p.h>

#include <private/qv4value_p.h>
#include <private/qv4context_p.h>

QT_BEGIN_NAMESPACE

class QQmlEngine;
#if QT_CONFIG(qml_network)
class QNetworkAccessManager;
#endif
class QNetworkReply;
class QV4Include : public QObject
{
    Q_OBJECT
public:
    enum Status {
        Ok = 0,
        Loading = 1,
        NetworkError = 2,
        Exception = 3
    };

    static QV4::ReturnedValue method_include(const QV4::FunctionObject *, const QV4::Value *thisObject, const QV4::Value *argv, int argc);

private Q_SLOTS:
    void finished();

private:
    QV4Include(const QUrl &url, QV4::ExecutionEngine *engine, QV4::QmlContext *qmlContext, const QV4::Value &callback);
    ~QV4Include();

    QV4::ReturnedValue result();

    static QV4::ReturnedValue resultValue(QV4::ExecutionEngine *v4, Status status = Loading,
                                          const QString &statusText = QString());
    static void callback(const QV4::Value &callback, const QV4::Value &status);

    QV4::ExecutionEngine *v4;
    QUrl m_url;

#if QT_CONFIG(qml_network)
    int m_redirectCount;
    QNetworkAccessManager *m_network;
    QPointer<QNetworkReply> m_reply;
#endif

    QV4::PersistentValue m_callbackFunction;
    QV4::PersistentValue m_resultObject;
    QV4::PersistentValue m_qmlContext;
};

QT_END_NAMESPACE

#endif

