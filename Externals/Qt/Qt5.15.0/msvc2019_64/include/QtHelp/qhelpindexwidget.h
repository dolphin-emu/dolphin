/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#ifndef QHELPINDEXWIDGET_H
#define QHELPINDEXWIDGET_H

#include <QtHelp/qhelp_global.h>

#include <QtCore/QUrl>
#include <QtCore/QStringListModel>
#include <QtWidgets/QListView>

QT_BEGIN_NAMESPACE


class QHelpEnginePrivate;
class QHelpEngineCore;
class QHelpIndexModelPrivate;
struct QHelpLink;

class QHELP_EXPORT QHelpIndexModel : public QStringListModel
{
    Q_OBJECT

public:
    void createIndex(const QString &customFilterName);
    QModelIndex filter(const QString &filter,
        const QString &wildcard = QString());

#if QT_DEPRECATED_SINCE(5, 15)
    QT_DEPRECATED_X("Use QHelpEngineCore::documentsForKeyword() instead")
    QMap<QString, QUrl> linksForKeyword(const QString &keyword) const;
#endif
    bool isCreatingIndex() const;
    QHelpEngineCore *helpEngine() const;

Q_SIGNALS:
    void indexCreationStarted();
    void indexCreated();

private Q_SLOTS:
    void insertIndices();

private:
    QHelpIndexModel(QHelpEnginePrivate *helpEngine);
    ~QHelpIndexModel();

    QHelpIndexModelPrivate *d;
    friend class QHelpEnginePrivate;
};

class QHELP_EXPORT QHelpIndexWidget : public QListView
{
    Q_OBJECT

Q_SIGNALS:
#if QT_DEPRECATED_SINCE(5, 15)
    QT_DEPRECATED_X("Use documentActivated() instead")
    void linkActivated(const QUrl &link, const QString &keyword);
    QT_DEPRECATED_X("Use documentsActivated() instead")
    void linksActivated(const QMap<QString, QUrl> &links,
        const QString &keyword);
#endif
    void documentActivated(const QHelpLink &document,
                           const QString &keyword);
    void documentsActivated(const QList<QHelpLink> &documents,
                            const QString &keyword);

public Q_SLOTS:
    void filterIndices(const QString &filter,
        const QString &wildcard = QString());
    void activateCurrentItem();

private Q_SLOTS:
    void showLink(const QModelIndex &index);

private:
    QHelpIndexWidget();
    friend class QHelpEngine;
};

QT_END_NAMESPACE

#endif
