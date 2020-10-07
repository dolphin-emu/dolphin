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

#ifndef QQMLFILE_H
#define QQMLFILE_H

#include <QtQml/qtqmlglobal.h>

QT_BEGIN_NAMESPACE

class QUrl;
class QString;
class QObject;
class QQmlEngine;
class QQmlFilePrivate;

class Q_QML_EXPORT QQmlFile
{
public:
    QQmlFile();
    QQmlFile(QQmlEngine *, const QUrl &);
    QQmlFile(QQmlEngine *, const QString &);
    ~QQmlFile();

    enum Status { Null, Ready, Error, Loading };

    bool isNull() const;
    bool isReady() const;
    bool isError() const;
    bool isLoading() const;

    QUrl url() const;

    Status status() const;
    QString error() const;

    qint64 size() const;
    const char *data() const;
    QByteArray dataByteArray() const;

    void load(QQmlEngine *, const QUrl &);
    void load(QQmlEngine *, const QString &);

    void clear();
    void clear(QObject *);

#if QT_CONFIG(qml_network)
    bool connectFinished(QObject *, const char *);
    bool connectFinished(QObject *, int);
    bool connectDownloadProgress(QObject *, const char *);
    bool connectDownloadProgress(QObject *, int);
#endif

    static bool isSynchronous(const QString &url);
    static bool isSynchronous(const QUrl &url);

    static bool isLocalFile(const QString &url);
    static bool isLocalFile(const QUrl &url);

    static QString urlToLocalFileOrQrc(const QString &);
    static QString urlToLocalFileOrQrc(const QUrl &);
private:
    Q_DISABLE_COPY(QQmlFile)
    QQmlFilePrivate *d;
};

QT_END_NAMESPACE

#endif // QQMLFILE_H
