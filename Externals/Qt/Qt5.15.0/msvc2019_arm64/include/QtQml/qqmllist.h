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

#ifndef QQMLLIST_H
#define QQMLLIST_H

#include <QtQml/qtqmlglobal.h>
#include <QtCore/qlist.h>
#include <QtCore/qvariant.h>

QT_BEGIN_NAMESPACE


class QObject;
struct QMetaObject;

#ifndef QQMLLISTPROPERTY
#define QQMLLISTPROPERTY
template<typename T>
class QQmlListProperty {
public:
    using AppendFunction = void (*)(QQmlListProperty<T> *, T *);
    using CountFunction = int (*)(QQmlListProperty<T> *);
    using AtFunction = T *(*)(QQmlListProperty<T> *, int);
    using ClearFunction = void (*)(QQmlListProperty<T> *);
    using ReplaceFunction = void (*)(QQmlListProperty<T> *, int, T *);
    using RemoveLastFunction = void (*)(QQmlListProperty<T> *);

    QQmlListProperty() = default;

#if QT_DEPRECATED_SINCE(5,15)
    QT_DEPRECATED_X("Use constructor taking QList pointer, and gain improved performance")
    QQmlListProperty(QObject *o, QList<T *> &list)
        : object(o), data(&list), append(qlist_append), count(qlist_count), at(qlist_at),
          clear(qlist_clear), replace(qslow_replace), removeLast(qslow_removeLast)
    {}
#endif

    QQmlListProperty(QObject *o, QList<T *> *list)
        : object(o), data(list), append(qlist_append), count(qlist_count), at(qlist_at),
          clear(qlist_clear), replace(qlist_replace), removeLast(qlist_removeLast)
    {}

    QQmlListProperty(QObject *o, void *d, AppendFunction a, CountFunction c, AtFunction t,
                    ClearFunction r )
        : object(o),
          data(d),
          append(a),
          count(c),
          at(t),
          clear(r),
          replace((a && c && t && r) ? qslow_replace : nullptr),
          removeLast((a && c && t && r) ? qslow_removeLast : nullptr)
    {}

    QQmlListProperty(QObject *o, void *d, AppendFunction a, CountFunction c, AtFunction t,
                     ClearFunction r, ReplaceFunction s, RemoveLastFunction p)
        : object(o),
          data(d),
          append(a),
          count(c),
          at(t),
          clear((!r && p && c) ? qslow_clear : r),
          replace((!s && a && c && t && (r || p)) ? qslow_replace : s),
          removeLast((!p && a && c && t && r) ? qslow_removeLast : p)
    {}

    QQmlListProperty(QObject *o, void *d, CountFunction c, AtFunction a)
        : object(o), data(d), count(c), at(a)
    {}

    bool operator==(const QQmlListProperty &o) const {
        return object == o.object &&
               data == o.data &&
               append == o.append &&
               count == o.count &&
               at == o.at &&
               clear == o.clear &&
               replace == o.replace &&
               removeLast == o.removeLast;
    }

    QObject *object = nullptr;
    void *data = nullptr;

    AppendFunction append = nullptr;
    CountFunction count = nullptr;
    AtFunction at = nullptr;
    ClearFunction clear = nullptr;
    ReplaceFunction replace = nullptr;
    RemoveLastFunction removeLast = nullptr;

private:
    static void qlist_append(QQmlListProperty *p, T *v) {
        reinterpret_cast<QList<T *> *>(p->data)->append(v);
    }
    static int qlist_count(QQmlListProperty *p) {
        return reinterpret_cast<QList<T *> *>(p->data)->count();
    }
    static T *qlist_at(QQmlListProperty *p, int idx) {
        return reinterpret_cast<QList<T *> *>(p->data)->at(idx);
    }
    static void qlist_clear(QQmlListProperty *p) {
        return reinterpret_cast<QList<T *> *>(p->data)->clear();
    }
    static void qlist_replace(QQmlListProperty *p, int idx, T *v) {
        return reinterpret_cast<QList<T *> *>(p->data)->replace(idx, v);
    }
    static void qlist_removeLast(QQmlListProperty *p) {
        return reinterpret_cast<QList<T *> *>(p->data)->removeLast();
    }

    static void qslow_replace(QQmlListProperty<T> *list, int idx, T *v)
    {
        const int length = list->count(list);
        if (idx < 0 || idx >= length)
            return;

        QVector<T *> stash;
        if (list->clear != qslow_clear) {
            stash.reserve(length);
            for (int i = 0; i < length; ++i)
                stash.append(i == idx ? v : list->at(list, i));
            list->clear(list);
            for (T *item : qAsConst(stash))
                list->append(list, item);
        } else {
            stash.reserve(length - idx - 1);
            for (int i = length - 1; i > idx; --i) {
                stash.append(list->at(list, i));
                list->removeLast(list);
            }
            list->removeLast(list);
            list->append(list, v);
            while (!stash.isEmpty())
                list->append(list, stash.takeLast());
        }
    }

    static void qslow_clear(QQmlListProperty<T> *list)
    {
        for (int i = 0, end = list->count(list); i < end; ++i)
            list->removeLast(list);
    }

    static void qslow_removeLast(QQmlListProperty<T> *list)
    {
        const int length = list->count(list) - 1;
        if (length < 0)
            return;
        QVector<T *> stash;
        stash.reserve(length);
        for (int i = 0; i < length; ++i)
            stash.append(list->at(list, i));
        list->clear(list);
        for (T *item : qAsConst(stash))
            list->append(list, item);
    }
};
#endif

class QQmlEngine;
class QQmlListReferencePrivate;
class Q_QML_EXPORT QQmlListReference
{
public:
    QQmlListReference();
    QQmlListReference(QObject *, const char *property, QQmlEngine * = nullptr);
    QQmlListReference(const QQmlListReference &);
    QQmlListReference &operator=(const QQmlListReference &);
    ~QQmlListReference();

    bool isValid() const;

    QObject *object() const;
    const QMetaObject *listElementType() const;

    bool canAppend() const;
    bool canAt() const;
    bool canClear() const;
    bool canCount() const;
    bool canReplace() const;
    bool canRemoveLast() const;

    bool isManipulable() const;
    bool isReadable() const;

    bool append(QObject *) const;
    QObject *at(int) const;
    bool clear() const;
    int count() const;
    bool replace(int, QObject *) const;
    bool removeLast() const;

private:
    friend class QQmlListReferencePrivate;
    QQmlListReferencePrivate* d;
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QQmlListReference)

#endif // QQMLLIST_H
