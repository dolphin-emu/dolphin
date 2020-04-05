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

#ifndef QQMLTYPELOADERNETWORKREPLYPROXY_P_H
#define QQMLTYPELOADERNETWORKREPLYPROXY_P_H

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

#include <QtQml/qtqmlglobal.h>
#include <QtCore/qobject.h>

QT_REQUIRE_CONFIG(qml_network);

QT_BEGIN_NAMESPACE

class QNetworkReply;
class QQmlTypeLoader;

// This is a lame object that we need to ensure that slots connected to
// QNetworkReply get called in the correct thread (the loader thread).
// As QQmlTypeLoader lives in the main thread, and we can't use
// Qt::DirectConnection connections from a QNetworkReply (because then
// sender() wont work), we need to insert this object in the middle.
class QQmlTypeLoaderNetworkReplyProxy : public QObject
{
    Q_OBJECT
public:
    QQmlTypeLoaderNetworkReplyProxy(QQmlTypeLoader *l);

public slots:
    void finished();
    void downloadProgress(qint64, qint64);
    void manualFinished(QNetworkReply*);

private:
    QQmlTypeLoader *l;
};

QT_END_NAMESPACE

#endif // QQMLTYPELOADERNETWORKREPLYPROXY_P_H
