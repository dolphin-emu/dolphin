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

#ifndef QRECURSIONWATCHER_P_H
#define QRECURSIONWATCHER_P_H

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

class QRecursionNode;
class QRecursionNode {
public:
    inline QRecursionNode();
    bool *_r;
};

template<class T, QRecursionNode T::*Node>
class QRecursionWatcher {
public:
    inline QRecursionWatcher(T *);
    inline ~QRecursionWatcher();
    inline bool hasRecursed() const;
private:
    T *_t;
    bool _r;
};

QRecursionNode::QRecursionNode()
: _r(nullptr)
{
}

template<class T, QRecursionNode T::*Node>
QRecursionWatcher<T, Node>::QRecursionWatcher(T *t)
: _t(t), _r(false)
{
    if ((_t->*Node)._r) *(_t->*Node)._r = true;
    (_t->*Node)._r = &_r;
}

template<class T, QRecursionNode T::*Node>
QRecursionWatcher<T, Node>::~QRecursionWatcher()
{
    if ((_t->*Node)._r == &_r) (_t->*Node)._r = nullptr;
}

template<class T, QRecursionNode T::*Node>
bool QRecursionWatcher<T, Node>::hasRecursed() const
{
    return _r;
}

QT_END_NAMESPACE

#endif // QRECURSIONWATCHER_P_H
