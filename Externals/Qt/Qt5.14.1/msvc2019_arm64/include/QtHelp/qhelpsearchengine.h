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

#ifndef QHELPSEARCHENGINE_H
#define QHELPSEARCHENGINE_H

#include <QtHelp/qhelp_global.h>

#include <QtCore/QMap>
#include <QtCore/QUrl>
#include <QtCore/QObject>
#include <QtCore/QSharedDataPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>

QT_BEGIN_NAMESPACE

class QHelpEngineCore;
class QHelpSearchQueryWidget;
class QHelpSearchEnginePrivate;
class QHelpSearchResultData;
class QHelpSearchResultWidget;

class QHELP_EXPORT QHelpSearchQuery
{
public:
    enum FieldName { DEFAULT = 0, FUZZY, WITHOUT, PHRASE, ALL, ATLEAST };

    QHelpSearchQuery()
        : fieldName(DEFAULT) { wordList.clear(); }
    QHelpSearchQuery(FieldName field, const QStringList &wordList_)
        : fieldName(field), wordList(wordList_) {}

    FieldName fieldName;
    QStringList wordList;
};

class QHELP_EXPORT QHelpSearchResult
{
public:
    QHelpSearchResult();
    QHelpSearchResult(const QHelpSearchResult &other);
    QHelpSearchResult(const QUrl &url, const QString &title, const QString &snippet);
    ~QHelpSearchResult();

    QHelpSearchResult &operator=(const QHelpSearchResult &other);

    QString title() const;
    QUrl url() const;
    QString snippet() const;

private:
    QSharedDataPointer<QHelpSearchResultData> d;
};

class QHELP_EXPORT QHelpSearchEngine : public QObject
{
    Q_OBJECT

public:
    explicit QHelpSearchEngine(QHelpEngineCore *helpEngine, QObject *parent = nullptr);
    ~QHelpSearchEngine();

    QHelpSearchQueryWidget* queryWidget();
    QHelpSearchResultWidget* resultWidget();

#if QT_DEPRECATED_SINCE(5, 9)
    typedef QPair<QString, QString> SearchHit;

    QT_DEPRECATED int hitsCount() const;
    QT_DEPRECATED int hitCount() const;
    QT_DEPRECATED QList<SearchHit> hits(int start, int end) const;
    QT_DEPRECATED QList<QHelpSearchQuery> query() const;
#endif

    int searchResultCount() const;
    QVector<QHelpSearchResult> searchResults(int start, int end) const;
    QString searchInput() const;

public Q_SLOTS:
    void reindexDocumentation();
    void cancelIndexing();

#if QT_DEPRECATED_SINCE(5, 9)
    QT_DEPRECATED void search(const QList<QHelpSearchQuery> &queryList);
#endif

    void search(const QString &searchInput);
    void cancelSearching();

    void scheduleIndexDocumentation();

Q_SIGNALS:
    void indexingStarted();
    void indexingFinished();

    void searchingStarted();
    void searchingFinished(int searchResultCount);

private Q_SLOTS:
    void indexDocumentation();

private:
    QHelpSearchEnginePrivate *d;
};

QT_END_NAMESPACE

#endif  // QHELPSEARCHENGINE_H
