/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXml module of the Qt Toolkit.
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
#ifndef QDOM_P_H
#define QDOM_P_H

#include "qdom.h"

#include <qglobal.h>
#include <qhash.h>
#include <qstring.h>
#include <qlist.h>
#include <qxml.h>

QT_BEGIN_NAMESPACE

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists for the convenience of
// qxml.cpp and qdom.cpp. This header file may change from version to version without
// notice, or even be removed.
//
// We mean it.
//

/**************************************************************
 *
 * Private class declerations
 *
 **************************************************************/

class QDomImplementationPrivate
{
public:
    inline QDomImplementationPrivate() {}

    QDomImplementationPrivate *clone();
    QAtomicInt ref;
    static QDomImplementation::InvalidDataPolicy invalidDataPolicy;
};

class QDomNodePrivate
{
public:
    QDomNodePrivate(QDomDocumentPrivate *, QDomNodePrivate *parent = nullptr);
    QDomNodePrivate(QDomNodePrivate *n, bool deep);
    virtual ~QDomNodePrivate();

    QString nodeName() const { return name; }
    QString nodeValue() const { return value; }
    virtual void setNodeValue(const QString &v) { value = v; }

    QDomDocumentPrivate *ownerDocument();
    void setOwnerDocument(QDomDocumentPrivate *doc);

    virtual QDomNodePrivate *insertBefore(QDomNodePrivate *newChild, QDomNodePrivate *refChild);
    virtual QDomNodePrivate *insertAfter(QDomNodePrivate *newChild, QDomNodePrivate *refChild);
    virtual QDomNodePrivate *replaceChild(QDomNodePrivate *newChild, QDomNodePrivate *oldChild);
    virtual QDomNodePrivate *removeChild(QDomNodePrivate *oldChild);
    virtual QDomNodePrivate *appendChild(QDomNodePrivate *newChild);

    QDomNodePrivate *namedItem(const QString &name);

    virtual QDomNodePrivate *cloneNode(bool deep = true);
    virtual void normalize();
    virtual void clear();

    inline QDomNodePrivate *parent() const { return hasParent ? ownerNode : nullptr; }
    inline void setParent(QDomNodePrivate *p)
    {
        ownerNode = p;
        hasParent = true;
    }

    void setNoParent()
    {
        ownerNode = hasParent ? (QDomNodePrivate *)ownerDocument() : nullptr;
        hasParent = false;
    }

    // Dynamic cast
    bool isAttr() const { return nodeType() == QDomNode::AttributeNode; }
    bool isCDATASection() const { return nodeType() == QDomNode::CDATASectionNode; }
    bool isDocumentFragment() const { return nodeType() == QDomNode::DocumentFragmentNode; }
    bool isDocument() const { return nodeType() == QDomNode::DocumentNode; }
    bool isDocumentType() const { return nodeType() == QDomNode::DocumentTypeNode; }
    bool isElement() const { return nodeType() == QDomNode::ElementNode; }
    bool isEntityReference() const { return nodeType() == QDomNode::EntityReferenceNode; }
    bool isText() const
    {
        const QDomNode::NodeType nt = nodeType();
        return (nt == QDomNode::TextNode) || (nt == QDomNode::CDATASectionNode);
    }
    bool isEntity() const { return nodeType() == QDomNode::EntityNode; }
    bool isNotation() const { return nodeType() == QDomNode::NotationNode; }
    bool isProcessingInstruction() const
    {
        return nodeType() == QDomNode::ProcessingInstructionNode;
    }
    bool isCharacterData() const
    {
        const QDomNode::NodeType nt = nodeType();
        return (nt == QDomNode::CharacterDataNode) || (nt == QDomNode::TextNode)
                || (nt == QDomNode::CommentNode);
    }
    bool isComment() const { return nodeType() == QDomNode::CommentNode; }

    virtual QDomNode::NodeType nodeType() const { return QDomNode::BaseNode; }

    virtual void save(QTextStream &, int, int) const;

    void setLocation(int lineNumber, int columnNumber);

    // Variables
    QAtomicInt ref;
    QDomNodePrivate *prev;
    QDomNodePrivate *next;
    QDomNodePrivate *ownerNode; // either the node's parent or the node's owner document
    QDomNodePrivate *first;
    QDomNodePrivate *last;

    QString name; // this is the local name if prefix != null
    QString value;
    QString prefix; // set this only for ElementNode and AttributeNode
    QString namespaceURI; // set this only for ElementNode and AttributeNode
    bool createdWithDom1Interface : 1;
    bool hasParent : 1;

    int lineNumber;
    int columnNumber;
};

class QDomNodeListPrivate
{
public:
    QDomNodeListPrivate(QDomNodePrivate *);
    QDomNodeListPrivate(QDomNodePrivate *, const QString &);
    QDomNodeListPrivate(QDomNodePrivate *, const QString &, const QString &);
    ~QDomNodeListPrivate();

    bool operator==(const QDomNodeListPrivate &) const;
    bool operator!=(const QDomNodeListPrivate &) const;

    void createList();
    QDomNodePrivate *item(int index);
    int length() const;

    QAtomicInt ref;
    /*
      This list contains the children of this node.
     */
    QDomNodePrivate *node_impl;
    QString tagname;
    QString nsURI;
    QList<QDomNodePrivate *> list;
    long timestamp;
};

class QDomNamedNodeMapPrivate
{
public:
    QDomNamedNodeMapPrivate(QDomNodePrivate *);
    ~QDomNamedNodeMapPrivate();

    QDomNodePrivate *namedItem(const QString &name) const;
    QDomNodePrivate *namedItemNS(const QString &nsURI, const QString &localName) const;
    QDomNodePrivate *setNamedItem(QDomNodePrivate *arg);
    QDomNodePrivate *setNamedItemNS(QDomNodePrivate *arg);
    QDomNodePrivate *removeNamedItem(const QString &name);
    QDomNodePrivate *item(int index) const;
    int length() const;
    bool contains(const QString &name) const;
    bool containsNS(const QString &nsURI, const QString &localName) const;

    /**
     * Remove all children from the map.
     */
    void clearMap();
    bool isReadOnly() { return readonly; }
    void setReadOnly(bool r) { readonly = r; }
    bool isAppendToParent() { return appendToParent; }
    /**
     * If true, then the node will redirect insert/remove calls
     * to its parent by calling QDomNodePrivate::appendChild or removeChild.
     * In addition the map won't increase or decrease the reference count
     * of the nodes it contains.
     *
     * By default this value is false and the map will handle reference counting
     * by itself.
     */
    void setAppendToParent(bool b) { appendToParent = b; }

    /**
     * Creates a copy of the map. It is a deep copy
     * that means that all children are cloned.
     */
    QDomNamedNodeMapPrivate *clone(QDomNodePrivate *parent);

    // Variables
    QAtomicInt ref;
    QMultiHash<QString, QDomNodePrivate *> map;
    QDomNodePrivate *parent;
    bool readonly;
    bool appendToParent;
};

class QDomDocumentTypePrivate : public QDomNodePrivate
{
public:
    QDomDocumentTypePrivate(QDomDocumentPrivate *, QDomNodePrivate *parent = nullptr);
    QDomDocumentTypePrivate(QDomDocumentTypePrivate *n, bool deep);
    ~QDomDocumentTypePrivate();
    void init();

    // Reimplemented from QDomNodePrivate
    QDomNodePrivate *cloneNode(bool deep = true) override;
    QDomNodePrivate *insertBefore(QDomNodePrivate *newChild, QDomNodePrivate *refChild) override;
    QDomNodePrivate *insertAfter(QDomNodePrivate *newChild, QDomNodePrivate *refChild) override;
    QDomNodePrivate *replaceChild(QDomNodePrivate *newChild, QDomNodePrivate *oldChild) override;
    QDomNodePrivate *removeChild(QDomNodePrivate *oldChild) override;
    QDomNodePrivate *appendChild(QDomNodePrivate *newChild) override;

    QDomNode::NodeType nodeType() const override { return QDomNode::DocumentTypeNode; }

    void save(QTextStream &s, int, int) const override;

    // Variables
    QDomNamedNodeMapPrivate *entities;
    QDomNamedNodeMapPrivate *notations;
    QString publicId;
    QString systemId;
    QString internalSubset;
};

class QDomDocumentFragmentPrivate : public QDomNodePrivate
{
public:
    QDomDocumentFragmentPrivate(QDomDocumentPrivate *, QDomNodePrivate *parent = nullptr);
    QDomDocumentFragmentPrivate(QDomNodePrivate *n, bool deep);

    // Reimplemented from QDomNodePrivate
    virtual QDomNodePrivate *cloneNode(bool deep = true) override;
    QDomNode::NodeType nodeType() const override { return QDomNode::DocumentFragmentNode; }
};

class QDomCharacterDataPrivate : public QDomNodePrivate
{
public:
    QDomCharacterDataPrivate(QDomDocumentPrivate *, QDomNodePrivate *parent, const QString &data);
    QDomCharacterDataPrivate(QDomCharacterDataPrivate *n, bool deep);

    int dataLength() const;
    QString substringData(unsigned long offset, unsigned long count) const;
    void appendData(const QString &arg);
    void insertData(unsigned long offset, const QString &arg);
    void deleteData(unsigned long offset, unsigned long count);
    void replaceData(unsigned long offset, unsigned long count, const QString &arg);

    // Reimplemented from QDomNodePrivate
    QDomNode::NodeType nodeType() const override { return QDomNode::CharacterDataNode; }
    QDomNodePrivate *cloneNode(bool deep = true) override;
};

class QDomTextPrivate : public QDomCharacterDataPrivate
{
public:
    QDomTextPrivate(QDomDocumentPrivate *, QDomNodePrivate *parent, const QString &val);
    QDomTextPrivate(QDomTextPrivate *n, bool deep);

    QDomTextPrivate *splitText(int offset);

    // Reimplemented from QDomNodePrivate
    QDomNodePrivate *cloneNode(bool deep = true) override;
    QDomNode::NodeType nodeType() const override { return QDomNode::TextNode; }
    virtual void save(QTextStream &s, int, int) const override;
};

class QDomAttrPrivate : public QDomNodePrivate
{
public:
    QDomAttrPrivate(QDomDocumentPrivate *, QDomNodePrivate *, const QString &name);
    QDomAttrPrivate(QDomDocumentPrivate *, QDomNodePrivate *, const QString &nsURI,
                    const QString &qName);
    QDomAttrPrivate(QDomAttrPrivate *n, bool deep);

    bool specified() const;

    // Reimplemented from QDomNodePrivate
    void setNodeValue(const QString &v) override;
    QDomNodePrivate *cloneNode(bool deep = true) override;
    QDomNode::NodeType nodeType() const override { return QDomNode::AttributeNode; }
    virtual void save(QTextStream &s, int, int) const override;

    // Variables
    bool m_specified;
};

class QDomElementPrivate : public QDomNodePrivate
{
public:
    QDomElementPrivate(QDomDocumentPrivate *, QDomNodePrivate *parent, const QString &name);
    QDomElementPrivate(QDomDocumentPrivate *, QDomNodePrivate *parent, const QString &nsURI,
                       const QString &qName);
    QDomElementPrivate(QDomElementPrivate *n, bool deep);
    ~QDomElementPrivate();

    QString attribute(const QString &name, const QString &defValue) const;
    QString attributeNS(const QString &nsURI, const QString &localName,
                        const QString &defValue) const;
    void setAttribute(const QString &name, const QString &value);
    void setAttributeNS(const QString &nsURI, const QString &qName, const QString &newValue);
    void removeAttribute(const QString &name);
    QDomAttrPrivate *attributeNode(const QString &name);
    QDomAttrPrivate *attributeNodeNS(const QString &nsURI, const QString &localName);
    QDomAttrPrivate *setAttributeNode(QDomAttrPrivate *newAttr);
    QDomAttrPrivate *setAttributeNodeNS(QDomAttrPrivate *newAttr);
    QDomAttrPrivate *removeAttributeNode(QDomAttrPrivate *oldAttr);
    bool hasAttribute(const QString &name);
    bool hasAttributeNS(const QString &nsURI, const QString &localName);

    QString text();

    // Reimplemented from QDomNodePrivate
    QDomNamedNodeMapPrivate *attributes() { return m_attr; }
    bool hasAttributes() { return (m_attr->length() > 0); }
    QDomNode::NodeType nodeType() const override { return QDomNode::ElementNode; }
    QDomNodePrivate *cloneNode(bool deep = true) override;
    virtual void save(QTextStream &s, int, int) const override;

    // Variables
    QDomNamedNodeMapPrivate *m_attr;
};

class QDomCommentPrivate : public QDomCharacterDataPrivate
{
public:
    QDomCommentPrivate(QDomDocumentPrivate *, QDomNodePrivate *parent, const QString &val);
    QDomCommentPrivate(QDomCommentPrivate *n, bool deep);

    // Reimplemented from QDomNodePrivate
    QDomNodePrivate *cloneNode(bool deep = true) override;
    QDomNode::NodeType nodeType() const override { return QDomNode::CommentNode; }
    virtual void save(QTextStream &s, int, int) const override;
};

class QDomCDATASectionPrivate : public QDomTextPrivate
{
public:
    QDomCDATASectionPrivate(QDomDocumentPrivate *, QDomNodePrivate *parent, const QString &val);
    QDomCDATASectionPrivate(QDomCDATASectionPrivate *n, bool deep);

    // Reimplemented from QDomNodePrivate
    QDomNodePrivate *cloneNode(bool deep = true) override;
    QDomNode::NodeType nodeType() const override { return QDomNode::CDATASectionNode; }
    virtual void save(QTextStream &s, int, int) const override;
};

class QDomNotationPrivate : public QDomNodePrivate
{
public:
    QDomNotationPrivate(QDomDocumentPrivate *, QDomNodePrivate *parent, const QString &name,
                        const QString &pub, const QString &sys);
    QDomNotationPrivate(QDomNotationPrivate *n, bool deep);

    // Reimplemented from QDomNodePrivate
    QDomNodePrivate *cloneNode(bool deep = true) override;
    QDomNode::NodeType nodeType() const override { return QDomNode::NotationNode; }
    virtual void save(QTextStream &s, int, int) const override;

    // Variables
    QString m_sys;
    QString m_pub;
};

class QDomEntityPrivate : public QDomNodePrivate
{
public:
    QDomEntityPrivate(QDomDocumentPrivate *, QDomNodePrivate *parent, const QString &name,
                      const QString &pub, const QString &sys, const QString &notation);
    QDomEntityPrivate(QDomEntityPrivate *n, bool deep);

    // Reimplemented from QDomNodePrivate
    QDomNodePrivate *cloneNode(bool deep = true) override;
    QDomNode::NodeType nodeType() const override { return QDomNode::EntityNode; }
    virtual void save(QTextStream &s, int, int) const override;

    // Variables
    QString m_sys;
    QString m_pub;
    QString m_notationName;
};

class QDomEntityReferencePrivate : public QDomNodePrivate
{
public:
    QDomEntityReferencePrivate(QDomDocumentPrivate *, QDomNodePrivate *parent, const QString &name);
    QDomEntityReferencePrivate(QDomNodePrivate *n, bool deep);

    // Reimplemented from QDomNodePrivate
    QDomNodePrivate *cloneNode(bool deep = true) override;
    QDomNode::NodeType nodeType() const override { return QDomNode::EntityReferenceNode; }
    virtual void save(QTextStream &s, int, int) const override;
};

class QDomProcessingInstructionPrivate : public QDomNodePrivate
{
public:
    QDomProcessingInstructionPrivate(QDomDocumentPrivate *, QDomNodePrivate *parent,
                                     const QString &target, const QString &data);
    QDomProcessingInstructionPrivate(QDomProcessingInstructionPrivate *n, bool deep);

    // Reimplemented from QDomNodePrivate
    QDomNodePrivate *cloneNode(bool deep = true) override;
    QDomNode::NodeType nodeType() const override { return QDomNode::ProcessingInstructionNode; }
    virtual void save(QTextStream &s, int, int) const override;
};

class QDomDocumentPrivate : public QDomNodePrivate
{
public:
    QDomDocumentPrivate();
    QDomDocumentPrivate(const QString &name);
    QDomDocumentPrivate(QDomDocumentTypePrivate *dt);
    QDomDocumentPrivate(QDomDocumentPrivate *n, bool deep);
    ~QDomDocumentPrivate();

#if QT_DEPRECATED_SINCE(5, 15)
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
    bool setContent(QXmlInputSource *source, bool namespaceProcessing, QString *errorMsg,
                    int *errorLine, int *errorColumn);
    bool setContent(QXmlInputSource *source, QXmlReader *reader, QXmlSimpleReader *simpleReader,
                    QString *errorMsg, int *errorLine, int *errorColumn);
QT_WARNING_POP
#endif
    bool setContent(QXmlStreamReader *reader, bool namespaceProcessing, QString *errorMsg,
                    int *errorLine, int *errorColumn);

    // Attributes
    QDomDocumentTypePrivate *doctype() { return type.data(); }
    QDomImplementationPrivate *implementation() { return impl.data(); }
    QDomElementPrivate *documentElement();

    // Factories
    QDomElementPrivate *createElement(const QString &tagName);
    QDomElementPrivate *createElementNS(const QString &nsURI, const QString &qName);
    QDomDocumentFragmentPrivate *createDocumentFragment();
    QDomTextPrivate *createTextNode(const QString &data);
    QDomCommentPrivate *createComment(const QString &data);
    QDomCDATASectionPrivate *createCDATASection(const QString &data);
    QDomProcessingInstructionPrivate *createProcessingInstruction(const QString &target,
                                                                  const QString &data);
    QDomAttrPrivate *createAttribute(const QString &name);
    QDomAttrPrivate *createAttributeNS(const QString &nsURI, const QString &qName);
    QDomEntityReferencePrivate *createEntityReference(const QString &name);

    QDomNodePrivate *importNode(QDomNodePrivate *importedNode, bool deep);

    // Reimplemented from QDomNodePrivate
    QDomNodePrivate *cloneNode(bool deep = true) override;
    QDomNode::NodeType nodeType() const override { return QDomNode::DocumentNode; }
    void clear() override;

    // Variables
    QExplicitlySharedDataPointer<QDomImplementationPrivate> impl;
    QExplicitlySharedDataPointer<QDomDocumentTypePrivate> type;

    void saveDocument(QTextStream &stream, const int indent,
                      QDomNode::EncodingPolicy encUsed) const;

    /* \internal
       Counter for the QDomNodeListPrivate timestamps.

       This is a cache optimization, that might in some cases be effective. The
       dilemma is that QDomNode::childNodes() returns a list, but the
       implementation stores the children in a linked list. Hence, in order to
       get the children out through childNodes(), a list must be populated each
       time, which is O(N).

       DOM has the requirement of node references being live, see DOM Core
       Level 3, 1.1.1 The DOM Structure Model, which means that changes to the
       underlying documents must be reflected in node lists.

       This mechanism, nodeListTime, is a caching optimization that reduces the
       amount of times the node list is rebuilt, by only doing so when the
       document actually changes. However, a change to anywhere in any document
       invalidate all lists, since no dependency tracking is done.

       It functions by that all modifying functions(insertBefore() and so on)
       increment the count; each QDomNodeListPrivate copies nodeListTime on
       construction, and compares its own value to nodeListTime in order to
       determine whether it needs to rebuild.

       This is reentrant. The nodeListTime may overflow, but that's ok since we
       check for equalness, not whether nodeListTime is smaller than the list's
       stored timestamp.
    */
    long nodeListTime;
};

QT_END_NAMESPACE

#endif // QDOMHELPERS_P_H
