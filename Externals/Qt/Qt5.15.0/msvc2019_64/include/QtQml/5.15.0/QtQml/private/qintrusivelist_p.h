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

#ifndef QINTRUSIVELIST_P_H
#define QINTRUSIVELIST_P_H

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

#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE

class QIntrusiveListNode;
template<class N, QIntrusiveListNode N::*member>
class QIntrusiveList
{
public:
    inline QIntrusiveList();
    inline ~QIntrusiveList();

    inline bool isEmpty() const;
    inline void insert(N *n);
    inline void remove(N *n);
    inline bool contains(N *) const;

    class iterator {
    public:
        inline iterator();
        inline iterator(N *value);

        inline N *operator*() const;
        inline N *operator->() const;
        inline bool operator==(const iterator &other) const;
        inline bool operator!=(const iterator &other) const;
        inline iterator &operator++();

        inline iterator &erase();

    private:
        N *_value;
    };
    typedef iterator Iterator;

    inline N *first() const;
    static inline N *next(N *current);

    inline iterator begin();
    inline iterator end();

private:
    static inline N *nodeToN(QIntrusiveListNode *node);

    QIntrusiveListNode *__first = nullptr;
};

class QIntrusiveListNode
{
public:
    inline QIntrusiveListNode();
    inline ~QIntrusiveListNode();

    inline void remove();
    inline bool isInList() const;

    QIntrusiveListNode *_next = nullptr;
    QIntrusiveListNode**_prev = nullptr;
};

template<class N, QIntrusiveListNode N::*member>
QIntrusiveList<N, member>::iterator::iterator()
: _value(nullptr)
{
}

template<class N, QIntrusiveListNode N::*member>
QIntrusiveList<N, member>::iterator::iterator(N *value)
: _value(value)
{
}

template<class N, QIntrusiveListNode N::*member>
N *QIntrusiveList<N, member>::iterator::operator*() const
{
    return _value;
}

template<class N, QIntrusiveListNode N::*member>
N *QIntrusiveList<N, member>::iterator::operator->() const
{
    return _value;
}

template<class N, QIntrusiveListNode N::*member>
bool QIntrusiveList<N, member>::iterator::operator==(const iterator &other) const
{
    return other._value == _value;
}

template<class N, QIntrusiveListNode N::*member>
bool QIntrusiveList<N, member>::iterator::operator!=(const iterator &other) const
{
    return other._value != _value;
}

template<class N, QIntrusiveListNode N::*member>
typename QIntrusiveList<N, member>::iterator &QIntrusiveList<N, member>::iterator::operator++()
{
    _value = QIntrusiveList<N, member>::next(_value);
    return *this;
}

template<class N, QIntrusiveListNode N::*member>
typename QIntrusiveList<N, member>::iterator &QIntrusiveList<N, member>::iterator::erase()
{
    N *old = _value;
    _value = QIntrusiveList<N, member>::next(_value);
    (old->*member).remove();
    return *this;
}

template<class N, QIntrusiveListNode N::*member>
QIntrusiveList<N, member>::QIntrusiveList()

{
}

template<class N, QIntrusiveListNode N::*member>
QIntrusiveList<N, member>::~QIntrusiveList()
{
    while (__first) __first->remove();
}

template<class N, QIntrusiveListNode N::*member>
bool QIntrusiveList<N, member>::isEmpty() const
{
    return __first == nullptr;
}

template<class N, QIntrusiveListNode N::*member>
void QIntrusiveList<N, member>::insert(N *n)
{
    QIntrusiveListNode *nnode = &(n->*member);
    nnode->remove();

    nnode->_next = __first;
    if (nnode->_next) nnode->_next->_prev = &nnode->_next;
    __first = nnode;
    nnode->_prev = &__first;
}

template<class N, QIntrusiveListNode N::*member>
void QIntrusiveList<N, member>::remove(N *n)
{
    QIntrusiveListNode *nnode = &(n->*member);
    nnode->remove();
}

template<class N, QIntrusiveListNode N::*member>
bool QIntrusiveList<N, member>::contains(N *n) const
{
    QIntrusiveListNode *nnode = __first;
    while (nnode) {
        if (nodeToN(nnode) == n)
            return true;
        nnode = nnode->_next;
    }
    return false;
}

template<class N, QIntrusiveListNode N::*member>
N *QIntrusiveList<N, member>::first() const
{
    return __first?nodeToN(__first):nullptr;
}

template<class N, QIntrusiveListNode N::*member>
N *QIntrusiveList<N, member>::next(N *current)
{
    QIntrusiveListNode *nextnode = (current->*member)._next;
    N *nextstruct = nextnode?nodeToN(nextnode):nullptr;
    return nextstruct;
}

template<class N, QIntrusiveListNode N::*member>
typename QIntrusiveList<N, member>::iterator QIntrusiveList<N, member>::begin()
{
    return __first?iterator(nodeToN(__first)):iterator();
}

template<class N, QIntrusiveListNode N::*member>
typename QIntrusiveList<N, member>::iterator QIntrusiveList<N, member>::end()
{
    return iterator();
}

template<class N, QIntrusiveListNode N::*member>
N *QIntrusiveList<N, member>::nodeToN(QIntrusiveListNode *node)
{
    return (N *)((char *)node - ((char *)&(((N *)nullptr)->*member) - (char *)nullptr));
}

QIntrusiveListNode::QIntrusiveListNode()
{
}

QIntrusiveListNode::~QIntrusiveListNode()
{
    remove();
}

void QIntrusiveListNode::remove()
{
    if (_prev) *_prev = _next;
    if (_next) _next->_prev = _prev;
    _prev = nullptr;
    _next = nullptr;
}

bool QIntrusiveListNode::isInList() const
{
    return _prev != nullptr;
}

QT_END_NAMESPACE

#endif // QINTRUSIVELIST_P_H
