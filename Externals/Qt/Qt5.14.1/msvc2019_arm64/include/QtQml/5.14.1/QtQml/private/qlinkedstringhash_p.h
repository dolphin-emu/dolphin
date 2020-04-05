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

#ifndef QLINKEDSTRINGHASH_P_H
#define QLINKEDSTRINGHASH_P_H

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

#include <private/qstringhash_p.h>

QT_BEGIN_NAMESPACE

template<class T>
class QLinkedStringHash : private QStringHash<T>
{
public:
    using typename QStringHash<T>::Node;
    using typename QStringHash<T>::NewedNode;
    using typename QStringHash<T>::ReservedNodePool;
    using typename QStringHash<T>::mapped_type;

    using ConstIteratorData = QStringHashData::IteratorData<const QLinkedStringHash>;
    using ConstIterator = typename QStringHash<T>::template Iterator<ConstIteratorData, const T>;

    void linkAndReserve(const QLinkedStringHash<T> &other, int additionalReserve)
    {
        clear();

        if (other.count()) {
            data.size = other.data.size;
            data.rehashToSize(other.count() + additionalReserve);

            if (data.numBuckets == other.data.numBuckets) {
                nodePool = new ReservedNodePool;
                nodePool->count = additionalReserve;
                nodePool->used = 0;
                nodePool->nodes = new Node[additionalReserve];

                for (int ii = 0; ii < data.numBuckets; ++ii)
                    data.buckets[ii] = (Node *)other.data.buckets[ii];

                link = &other;
                return;
            }

            data.size = 0;
        }

        data.numBits = other.data.numBits;
        reserve(other.count() + additionalReserve);
        copy(other);
    }

    inline bool isLinked() const
    {
        return link != 0;
    }

    void clear()
    {
        QStringHash<T>::clear();
        link = nullptr;
    }

    template<typename K>
    void insert(const K &key, const T &value)
    {
        // If this is a linked hash, we can't rely on owning the node, so we always
        // create a new one.
        Node *n = link ? nullptr : QStringHash<T>::findNode(key);
        if (n)
            n->value = value;
        else
            QStringHash<T>::createNode(key, value);
    }

    template<typename K>
    inline ConstIterator find(const K &key) const
    {
        return iterator(QStringHash<T>::findNode(key));
    }

    ConstIterator begin() const
    {
        return ConstIterator(
                QStringHash<T>::template iterateFirst<const QLinkedStringHash<T>,
                                                      ConstIteratorData>(this));
    }

    ConstIterator end() const { return ConstIterator(); }

    inline T *value(const ConstIterator &iter) { return value(iter.node()->key()); }

    using QStringHash<T>::value;
    using QStringHash<T>::reserve;
    using QStringHash<T>::copy;

protected:
    friend QStringHash<T>;
    using QStringHash<T>::data;
    using QStringHash<T>::nodePool;

    using QStringHash<T>::createNode;

    inline ConstIteratorData iterateFirst() const
    {
        const ConstIteratorData rv
                = QStringHash<T>::template iterateFirst<const QLinkedStringHash<T>,
                                                        ConstIteratorData>(this);
        return (rv.n == nullptr && link) ? link->iterateFirst() : rv;
    }

    static inline ConstIteratorData iterateNext(const ConstIteratorData &d)
    {
        const QLinkedStringHash<T> *self = d.p;
        const ConstIteratorData rv = QStringHash<T>::iterateNext(d);
        return (rv.n == nullptr && self->link) ? self->link->iterateFirst() : rv;
    }

    inline ConstIterator iterator(Node *n) const
    {
        if (!n)
            return ConstIterator();

        const QLinkedStringHash<T> *container = this;

        if (link) {
            // This node could be in the linked hash
            if ((n >= nodePool->nodes) && (n < (nodePool->nodes + nodePool->used))) {
                // The node is in this hash
            } else if ((n >= link->nodePool->nodes)
                       && (n < (link->nodePool->nodes + link->nodePool->used))) {
                // The node is in the linked hash
                container = link;
            } else {
                const NewedNode *ln = link->newedNodes;
                while (ln) {
                    if (ln == n) {
                        // This node is in the linked hash's newed list
                        container = link;
                        break;
                    }
                    ln = ln->nextNewed;
                }
            }
        }


        ConstIteratorData rv;
        rv.n = n;
        rv.p = container;
        return ConstIterator(rv);
    }

    const QLinkedStringHash<T> *link = nullptr;
};

template<class T>
class QLinkedStringMultiHash : public QLinkedStringHash<T>
{
public:
    using ConstIterator = typename QLinkedStringHash<T>::ConstIterator;

    template<typename K>
    inline void insert(const K &key, const T &value)
    {
        // Always create a new node
        QLinkedStringHash<T>::createNode(key, value);
    }

    inline void insert(const ConstIterator &iter)
    {
        // Always create a new node
        QLinkedStringHash<T>::createNode(iter.key(), iter.value());
    }

    inline ConstIterator findNext(const ConstIterator &iter) const
    {
        if (auto *node = iter.node()) {
            QHashedString key(node->key());
            while ((node = static_cast<typename QLinkedStringHash<T>::Node *>(*node->next))) {
                if (node->equals(key))
                    return QLinkedStringHash<T>::iterator(node);
            }
        }

        return ConstIterator();
    }
};

QT_END_NAMESPACE

#endif // QLINKEDSTRINGHASH_P_H
