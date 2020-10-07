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

#ifndef QSTRINGHASH_P_H
#define QSTRINGHASH_P_H

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

#include <private/qhashedstring_p.h>
#include <private/qprimefornumbits_p.h>

#include <QtCore/qglobal.h>

QT_BEGIN_NAMESPACE

class QStringHashData;
class QStringHashNode
{
public:
    QStringHashNode()
    : ckey(nullptr)
    {
    }

    QStringHashNode(const QHashedString &key)
    : length(key.length()), hash(key.hash()), symbolId(0)
    {
        strData = const_cast<QHashedString &>(key).data_ptr();
        setQString(true);
        strData->ref.ref();
    }

    QStringHashNode(const QHashedCStringRef &key)
    : length(key.length()), hash(key.hash()), symbolId(0), ckey(key.constData())
    {
    }

    QStringHashNode(const QStringHashNode &o)
    : length(o.length), hash(o.hash), symbolId(o.symbolId), ckey(o.ckey)
    {
        setQString(o.isQString());
        if (isQString()) { strData->ref.ref(); }
    }

    ~QStringHashNode()
    {
        if (isQString()) { if (!strData->ref.deref()) free(strData); }
    }

    QFlagPointer<QStringHashNode> next;

    qint32 length = 0;
    quint32 hash = 0;
    quint32 symbolId = 0;

    union {
        const char *ckey;
        QStringData *strData;
    };

    inline QHashedString key() const
    {
        if (isQString())
            return QHashedString(QString((QChar *)strData->data(), length), hash);

        return QHashedString(QString::fromLatin1(ckey, length), hash);
    }

    bool isQString() const { return next.flag(); }
    void setQString(bool v) { if (v) next.setFlag(); else next.clearFlag(); }

    inline char *cStrData() const { return (char *)ckey; }
    inline quint16 *utf16Data() const { return (quint16 *)strData->data(); }

    inline bool equals(const QV4::Value &string) const {
        QString s = string.toQStringNoThrow();
        if (isQString()) {
            QStringDataPtr dd;
            dd.ptr = strData;
            strData->ref.ref();
            return QString(dd) == s;
        } else {
            return QLatin1String(cStrData(), length) == s;
        }
    }

    inline bool equals(const QV4::String *string) const {
        if (length != string->d()->length() || hash != string->hashValue())
                return false;
        if (isQString()) {
            QStringDataPtr dd;
            dd.ptr = strData;
            strData->ref.ref();
            return QString(dd) == string->toQString();
        } else {
            return QLatin1String(cStrData(), length) == string->toQString();
        }
    }

    inline bool equals(const QHashedStringRef &string) const {
        return length == string.length() &&
               hash == string.hash() &&
               (isQString()?QHashedString::compare(string.constData(), (const QChar *)utf16Data(), length):
                            QHashedString::compare(string.constData(), cStrData(), length));
    }

    inline bool equals(const QHashedCStringRef &string) const {
        return length == string.length() &&
               hash == string.hash() &&
               (isQString()?QHashedString::compare((const QChar *)utf16Data(), string.constData(), length):
                            QHashedString::compare(string.constData(), cStrData(), length));
    }
};

class QStringHashData
{
    Q_DISABLE_COPY_MOVE(QStringHashData)
public:
    QStringHashData() = default;
    ~QStringHashData() = default;

    /*
        A QHash has initially around pow(2, MinNumBits) buckets. For
        example, if MinNumBits is 4, it has 17 buckets.
    */
    enum { MinNumBits = 4 };

    QStringHashNode **buckets = nullptr; // life cycle managed by QStringHash
    int numBuckets = 0;
    int size = 0;
    short numBits = 0;

    template<typename StringHash>
    struct IteratorData {
        IteratorData(QStringHashNode *n = nullptr, StringHash *p = nullptr) : n(n), p(p) {}

        template<typename OtherData>
        IteratorData(const OtherData &other) : n(other.n), p(other.p) {}

        QStringHashNode *n;
        StringHash *p;
    };

    void rehashToBits(short bits)
    {
        numBits = qMax(short(MinNumBits), bits);

        int nb = qPrimeForNumBits(numBits);
        if (nb == numBuckets && buckets)
            return;

        QStringHashNode **newBuckets = new QStringHashNode *[nb];
        ::memset(newBuckets, 0, sizeof(QStringHashNode *) * nb);

        // Preserve the existing order within buckets so that items with the
        // same key will retain the same find/findNext order
        for (int i = 0; i < numBuckets; ++i) {
            QStringHashNode *bucket = buckets[i];
            if (bucket)
                rehashNode(newBuckets, nb, bucket);
        }

        delete [] buckets;
        buckets = newBuckets;
        numBuckets = nb;
    }

    void rehashToSize(int size)
    {
        short bits = qMax(short(MinNumBits), numBits);
        while (qPrimeForNumBits(bits) < size)
            bits++;

        if (bits > numBits)
            rehashToBits(bits);
    }

    void rehashNode(QStringHashNode **newBuckets, int nb, QStringHashNode *node)
    {
        QStringHashNode *next = node->next.data();
        if (next)
            rehashNode(newBuckets, nb, next);

        int bucket = node->hash % nb;
        node->next = newBuckets[bucket];
        newBuckets[bucket] = node;
    }
};

// For a supplied key type, in what form do we need to keep a hashed version?
template<typename T>
struct HashedForm {};

template<> struct HashedForm<QString> { typedef QHashedString Type; };
template<> struct HashedForm<QStringRef> { typedef QHashedStringRef Type; };
template<> struct HashedForm<QHashedString> { typedef const QHashedString &Type; };
template<> struct HashedForm<QV4::String *> { typedef const QV4::String *Type; };
template<> struct HashedForm<const QV4::String *> { typedef const QV4::String *Type; };
template<> struct HashedForm<QHashedStringRef> { typedef const QHashedStringRef &Type; };
template<> struct HashedForm<QLatin1String> { typedef QHashedCStringRef Type; };
template<> struct HashedForm<QHashedCStringRef> { typedef const QHashedCStringRef &Type; };

class QStringHashBase
{
public:
    static HashedForm<QString>::Type hashedString(const QString &s) { return QHashedString(s);}
    static HashedForm<QStringRef>::Type hashedString(const QStringRef &s) { return QHashedStringRef(s.constData(), s.size());}
    static HashedForm<QHashedString>::Type hashedString(const QHashedString &s) { return s; }
    static HashedForm<QV4::String *>::Type hashedString(QV4::String *s) { return s; }
    static HashedForm<const QV4::String *>::Type hashedString(const QV4::String *s) { return s; }
    static HashedForm<QHashedStringRef>::Type hashedString(const QHashedStringRef &s) { return s; }

    static HashedForm<QLatin1String>::Type hashedString(const QLatin1String &s) { return QHashedCStringRef(s.data(), s.size()); }
    static HashedForm<QHashedCStringRef>::Type hashedString(const QHashedCStringRef &s) { return s; }

    static const QString &toQString(const QString &s) { return s; }
    static const QString &toQString(const QHashedString &s) { return s; }
    static QString toQString(const QV4::String *s) { return s->toQString(); }
    static QString toQString(const QHashedStringRef &s) { return s.toString(); }

    static QString toQString(const QLatin1String &s) { return QString(s); }
    static QString toQString(const QHashedCStringRef &s) { return s.toUtf16(); }

    static inline quint32 hashOf(const QHashedStringRef &s) { return s.hash(); }
    static inline quint32 hashOf(QV4::String *s) { return s->hashValue(); }
    static inline quint32 hashOf(const QV4::String *s) { return s->hashValue(); }

    template<typename K>
    static inline quint32 hashOf(const K &key) { return hashedString(key).hash(); }
};

template<class T>
class QStringHash : public QStringHashBase
{
public:
    typedef QHashedString key_type;
    typedef T mapped_type;

    using MutableIteratorData = QStringHashData::IteratorData<QStringHash<T>>;
    using ConstIteratorData = QStringHashData::IteratorData<const QStringHash<T>>;

    struct Node : public QStringHashNode {
        Node(const QHashedString &key, const T &value) : QStringHashNode(key), value(value) {}
        Node(const QHashedCStringRef &key, const T &value) : QStringHashNode(key), value(value) {}
        Node(const Node &o) : QStringHashNode(o), value(o.value) {}
        Node() {}
        T value;
    };
    struct NewedNode : public Node {
        NewedNode(const QHashedString &key, const T &value) : Node(key, value), nextNewed(nullptr) {}
        NewedNode(const QHashedCStringRef &key, const T &value) : Node(key, value), nextNewed(nullptr) {}
        NewedNode(const Node &o) : Node(o), nextNewed(nullptr) {}
        NewedNode *nextNewed;
    };
    struct ReservedNodePool
    {
        ReservedNodePool() : nodes(nullptr) {}
        ~ReservedNodePool() { delete [] nodes; }
        int count = 0;
        int used = 0;
        Node *nodes;
    };

    QStringHashData data;
    NewedNode *newedNodes;
    ReservedNodePool *nodePool;

    template<typename K>
    inline Node *findNode(const K &) const;

    inline Node *createNode(const Node &o);

    template<typename K>
    inline Node *createNode(const K &, const T &);

    inline Node *insertNode(Node *, quint32);

    inline void initializeNode(Node *, const QHashedString &key);
    inline void initializeNode(Node *, const QHashedCStringRef &key);

    template<typename K>
    inline Node *takeNode(const K &key, const T &value);

    inline Node *takeNode(const Node &o);

    inline void copy(const QStringHash<T> &);

    void copyNode(const QStringHashNode *otherNode);

    template<typename StringHash, typename Data>
    static inline Data iterateFirst(StringHash *self);

    template<typename Data>
    static inline Data iterateNext(const Data &);

public:
    inline QStringHash();
    inline QStringHash(const QStringHash &);
    inline ~QStringHash();

    QStringHash &operator=(const QStringHash<T> &);

    void copyAndReserve(const QStringHash<T> &other, int additionalReserve);

    inline bool isEmpty() const;
    inline void clear();
    inline int count() const;

    inline int numBuckets() const;

    template<typename Data, typename Value>
    class Iterator {
    public:
        inline Iterator() = default;
        inline Iterator(const Data &d) : d(d) {}

        inline Iterator &operator++()
        {
            d = QStringHash<T>::iterateNext(d);
            return *this;
        }

        inline bool operator==(const Iterator &o) const { return d.n == o.d.n; }
        inline bool operator!=(const Iterator &o) const { return d.n != o.d.n; }

        template<typename K>
        inline bool equals(const K &key) const { return d.n->equals(key); }

        inline QHashedString key() const { return static_cast<Node *>(d.n)->key(); }
        inline Value &value() const { return static_cast<Node *>(d.n)->value; }
        inline Value &operator*() const { return static_cast<Node *>(d.n)->value; }

        Node *node() const { return static_cast<Node *>(d.n); }
    private:
        Data d;
    };

    using MutableIterator = Iterator<MutableIteratorData, T>;
    using ConstIterator = Iterator<ConstIteratorData, const T>;

    template<typename K>
    inline void insert(const K &, const T &);
    inline void insert(const MutableIterator &);
    inline void insert(const ConstIterator &);

    template<typename K>
    inline T *value(const K &) const;
    inline T *value(const QV4::String *string) const;
    inline T *value(const MutableIterator &) const;
    inline T *value(const ConstIterator &) const;

    template<typename K>
    inline bool contains(const K &) const;

    template<typename K>
    inline T &operator[](const K &);

    inline MutableIterator begin();
    inline ConstIterator begin() const;
    inline ConstIterator constBegin() const { return begin(); }

    inline MutableIterator end();
    inline ConstIterator end() const;
    inline ConstIterator constEnd() const { return end(); }

    template<typename K>
    inline MutableIterator find(const K &);

    template<typename K>
    inline ConstIterator find(const K &) const;

    inline void reserve(int);
};

template<class T>
QStringHash<T>::QStringHash()
: newedNodes(nullptr), nodePool(nullptr)
{
}

template<class T>
QStringHash<T>::QStringHash(const QStringHash<T> &other)
: newedNodes(nullptr), nodePool(nullptr)
{
    data.numBits = other.data.numBits;
    data.size = other.data.size;
    reserve(other.count());
    copy(other);
}

template<class T>
QStringHash<T> &QStringHash<T>::operator=(const QStringHash<T> &other)
{
    if (&other == this)
        return *this;

    clear();

    data.numBits = other.data.numBits;
    data.size = other.data.size;
    reserve(other.count());
    copy(other);

    return *this;
}

template<class T>
void QStringHash<T>::copyAndReserve(const QStringHash<T> &other, int additionalReserve)
{
    clear();
    data.numBits = other.data.numBits;
    reserve(other.count() + additionalReserve);
    copy(other);
}

template<class T>
QStringHash<T>::~QStringHash()
{
    clear();
}

template<class T>
void QStringHash<T>::clear()
{
    // Delete the individually allocated nodes
    NewedNode *n = newedNodes;
    while (n) {
        NewedNode *c = n;
        n = c->nextNewed;
        delete c;
    }
    // Delete the pool allocated nodes
    if (nodePool) delete nodePool;
    delete [] data.buckets;

    data.buckets = nullptr;
    data.numBuckets = 0;
    data.numBits = 0;
    data.size = 0;

    newedNodes = nullptr;
    nodePool = nullptr;
}

template<class T>
bool QStringHash<T>::isEmpty() const
{
    return data.size== 0;
}

template<class T>
int QStringHash<T>::count() const
{
    return data.size;
}

template<class T>
int QStringHash<T>::numBuckets() const
{
    return data.numBuckets;
}

template<class T>
void QStringHash<T>::initializeNode(Node *node, const QHashedString &key)
{
    node->length = key.length();
    node->hash = key.hash();
    node->strData = const_cast<QHashedString &>(key).data_ptr();
    node->strData->ref.ref();
    node->setQString(true);
}

template<class T>
void QStringHash<T>::initializeNode(Node *node, const QHashedCStringRef &key)
{
    node->length = key.length();
    node->hash = key.hash();
    node->ckey = key.constData();
}

template<class T>
template<class K>
typename QStringHash<T>::Node *QStringHash<T>::takeNode(const K &key, const T &value)
{
    if (nodePool && nodePool->used != nodePool->count) {
        Node *rv = nodePool->nodes + nodePool->used++;
        initializeNode(rv, hashedString(key));
        rv->value = value;
        return rv;
    } else {
        NewedNode *rv = new NewedNode(hashedString(key), value);
        rv->nextNewed = newedNodes;
        newedNodes = rv;
        return rv;
    }
}

template<class T>
typename QStringHash<T>::Node *QStringHash<T>::takeNode(const Node &o)
{
    if (nodePool && nodePool->used != nodePool->count) {
        Node *rv = nodePool->nodes + nodePool->used++;
        rv->length = o.length;
        rv->hash = o.hash;
        if (o.isQString()) {
            rv->strData = o.strData;
            rv->strData->ref.ref();
            rv->setQString(true);
        } else {
            rv->ckey = o.ckey;
        }
        rv->symbolId = o.symbolId;
        rv->value = o.value;
        return rv;
    } else {
        NewedNode *rv = new NewedNode(o);
        rv->nextNewed = newedNodes;
        newedNodes = rv;
        return rv;
    }
}

template<class T>
void QStringHash<T>::copyNode(const QStringHashNode *otherNode)
{
    // Copy the predecessor before the successor
    QStringHashNode *next = otherNode->next.data();
    if (next)
        copyNode(next);

    Node *mynode = takeNode(*(const Node *)otherNode);
    int bucket = mynode->hash % data.numBuckets;
    mynode->next = data.buckets[bucket];
    data.buckets[bucket] = mynode;
}

template<class T>
void QStringHash<T>::copy(const QStringHash<T> &other)
{
    Q_ASSERT(data.size == 0);

    data.size = other.data.size;

    // Ensure buckets array is created
    data.rehashToBits(data.numBits);

    // Preserve the existing order within buckets
    for (int i = 0; i < other.data.numBuckets; ++i) {
        QStringHashNode *bucket = other.data.buckets[i];
        if (bucket)
            copyNode(bucket);
    }
}

template<class T>
template<typename Data>
Data QStringHash<T>::iterateNext(const Data &d)
{
    auto *This = d.p;
    Node *node = (Node *)d.n;

    if (This->nodePool && node >= This->nodePool->nodes &&
        node < (This->nodePool->nodes + This->nodePool->used)) {
        node--;
        if (node < This->nodePool->nodes)
            node = nullptr;
    } else {
        NewedNode *nn = (NewedNode *)node;
        node = nn->nextNewed;

        if (node == nullptr && This->nodePool && This->nodePool->used)
            node = This->nodePool->nodes + This->nodePool->used - 1;
    }

    Data rv;
    rv.n = node;
    rv.p = d.p;
    return rv;
}

template<class T>
template<typename StringHash, typename Data>
Data QStringHash<T>::iterateFirst(StringHash *self)
{
    typename StringHash::Node *n = nullptr;
    if (self->newedNodes)
        n = self->newedNodes;
    else if (self->nodePool && self->nodePool->used)
        n = self->nodePool->nodes + self->nodePool->used - 1;

    Data rv;
    rv.n = n;
    rv.p = self;
    return rv;
}

template<class T>
typename QStringHash<T>::Node *QStringHash<T>::createNode(const Node &o)
{
    Node *n = takeNode(o);
    return insertNode(n, n->hash);
}

template<class T>
template<class K>
typename QStringHash<T>::Node *QStringHash<T>::createNode(const K &key, const T &value)
{
    Node *n = takeNode(key, value);
    return insertNode(n, hashOf(key));
}

template<class T>
typename QStringHash<T>::Node *QStringHash<T>::insertNode(Node *n, quint32 hash)
{
    if (data.size >= data.numBuckets)
        data.rehashToBits(data.numBits + 1);

    int bucket = hash % data.numBuckets;
    n->next = data.buckets[bucket];
    data.buckets[bucket] = n;

    data.size++;

    return n;
}

template<class T>
template<class K>
void QStringHash<T>::insert(const K &key, const T &value)
{
    Node *n = findNode(key);
    if (n)
        n->value = value;
    else
        createNode(key, value);
}

template<class T>
void QStringHash<T>::insert(const MutableIterator &iter)
{
    insert(iter.key(), iter.value());
}

template<class T>
void QStringHash<T>::insert(const ConstIterator &iter)
{
    insert(iter.key(), iter.value());
}

template<class T>
template<class K>
typename QStringHash<T>::Node *QStringHash<T>::findNode(const K &key) const
{
    QStringHashNode *node = data.numBuckets?data.buckets[hashOf(key) % data.numBuckets]:nullptr;

    typename HashedForm<K>::Type hashedKey(hashedString(key));
    while (node && !node->equals(hashedKey))
        node = (*node->next);

    return (Node *)node;
}

template<class T>
template<class K>
T *QStringHash<T>::value(const K &key) const
{
    Node *n = findNode(key);
    return n?&n->value:nullptr;
}

template<typename T>
T *QStringHash<T>::value(const MutableIterator &iter) const
{
    return value(iter.node()->key());
}

template<class T>
T *QStringHash<T>::value(const ConstIterator &iter) const
{
    return value(iter.node()->key());
}

template<class T>
T *QStringHash<T>::value(const QV4::String *string) const
{
    Node *n = findNode(string);
    return n?&n->value:nullptr;
}

template<class T>
template<class K>
bool QStringHash<T>::contains(const K &key) const
{
    return nullptr != value(key);
}

template<class T>
template<class K>
T &QStringHash<T>::operator[](const K &key)
{
    Node *n = findNode(key);
    if (n) return n->value;
    else return createNode(key, T())->value;
}

template<class T>
void QStringHash<T>::reserve(int n)
{
    if (nodePool || 0 == n)
        return;

    nodePool = new ReservedNodePool;
    nodePool->count = n;
    nodePool->used = 0;
    nodePool->nodes = new Node[n];

    data.rehashToSize(n);
}

template<class T>
typename QStringHash<T>::MutableIterator QStringHash<T>::begin()
{
    return MutableIterator(iterateFirst<QStringHash<T>, MutableIteratorData>(this));
}

template<class T>
typename QStringHash<T>::ConstIterator QStringHash<T>::begin() const
{
    return ConstIterator(iterateFirst<const QStringHash<T>, ConstIteratorData>(this));
}

template<class T>
typename QStringHash<T>::MutableIterator QStringHash<T>::end()
{
    return MutableIterator();
}

template<class T>
typename QStringHash<T>::ConstIterator QStringHash<T>::end() const
{
    return ConstIterator();
}

template<class T>
template<class K>
typename QStringHash<T>::MutableIterator QStringHash<T>::find(const K &key)
{
    Node *n = findNode(key);
    return n ? MutableIterator(MutableIteratorData(n, this)) : MutableIterator();
}

template<class T>
template<class K>
typename QStringHash<T>::ConstIterator QStringHash<T>::find(const K &key) const
{
    Node *n = findNode(key);
    return n ? ConstIterator(ConstIteratorData(n, this)) : ConstIterator();
}

QT_END_NAMESPACE

#endif // QSTRINGHASH_P_H
