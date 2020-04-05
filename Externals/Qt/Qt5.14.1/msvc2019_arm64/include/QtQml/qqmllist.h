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
    typedef void (*AppendFunction)(QQmlListProperty<T> *, T*);
    typedef int (*CountFunction)(QQmlListProperty<T> *);
    typedef T *(*AtFunction)(QQmlListProperty<T> *, int);
    typedef void (*ClearFunction)(QQmlListProperty<T> *);

    QQmlListProperty()
        : append(nullptr),
          count(nullptr),
          at(nullptr),
          clear(nullptr)
    {}
    QQmlListProperty(QObject *o, QList<T *> &list)
        : object(o), data(&list), append(qlist_append), count(qlist_count), at(qlist_at),
          clear(qlist_clear)

    {}
    QQmlListProperty(QObject *o, void *d, AppendFunction a, CountFunction c, AtFunction t,
                    ClearFunction r )
        : object(o),
          data(d),
          append(a),
          count(c),
          at(t),
          clear(r)

    {}
    QQmlListProperty(QObject *o, void *d, CountFunction c, AtFunction t)
        : object(o),
          data(d),
          append(nullptr),
          count(c), at(t),
          clear(nullptr)
    {}
    bool operator==(const QQmlListProperty &o) const {
        return object == o.object &&
               data == o.data &&
               append == o.append &&
               count == o.count &&
               at == o.at &&
               clear == o.clear;
    }

    QObject *object = nullptr;
    void *data = nullptr;

    AppendFunction append;

    CountFunction count;
    AtFunction at;

    ClearFunction clear;

    void *dummy1 = nullptr;
    void *dummy2 = nullptr;

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

    bool isManipulable() const;
    bool isReadable() const;

    bool append(QObject *) const;
    QObject *at(int) const;
    bool clear() const;
    int count() const;

private:
    friend class QQmlListReferencePrivate;
    QQmlListReferencePrivate* d;
};

QT_END_NAMESPACE

Q_DECLARE_METATYPE(QQmlListReference)

#endif // QQMLLIST_H
