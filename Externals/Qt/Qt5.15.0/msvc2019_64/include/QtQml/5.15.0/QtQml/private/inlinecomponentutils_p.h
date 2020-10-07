/****************************************************************************
**
** Copyright (C) 2020 The Qt Company Ltd.
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
#ifndef INLINECOMPONENTUTILS_P_H
#define INLINECOMPONENTUTILS_P_H

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

#include <private/qv4compileddata_p.h>
#include <private/qv4executablecompilationunit_p.h>

namespace icutils {
struct Node {
    Node() = default;
    Node(const Node &) = default;
    Node(Node &&) = default;
    Node& operator=(Node const &) = default;
    Node& operator=(Node &&) = default;
    bool operator==(Node const &other) const {return index == other.index;}

    Node(std::vector<QV4::CompiledData::InlineComponent>::size_type s) {
        index = quint32(s);
        temporaryMark = 0;
        permanentMark = 0;
    }

    union {
        quint32_le_bitfield<0, 30> index;
        quint32_le_bitfield<30, 1> temporaryMark;
        quint32_le_bitfield<31, 1> permanentMark;
    };
};

using AdjacencyList = std::vector<std::vector<Node*>>;

template<typename ObjectContainer, typename InlineComponent>
void fillAdjacencyListForInlineComponents(ObjectContainer *objectContainer, AdjacencyList &adjacencyList, std::vector<Node> &nodes, const std::vector<InlineComponent> &allICs) {
    using CompiledObject = typename ObjectContainer::CompiledObject;
    // add an edge from A to B if A and B are inline components with the same containing type
    // and A inherits from B (ignore indirect chains through external types for now)
    // or if A instantiates B
    for (typename std::vector<InlineComponent>::size_type i = 0; i < allICs.size(); ++i) {
        const auto& ic = allICs[i];
        const CompiledObject *obj = objectContainer->objectAt(ic.objectIndex);
        QV4::ResolvedTypeReference *currentICTypeRef = objectContainer->resolvedType(ic.nameIndex);
        auto createEdgeFromTypeRef = [&](QV4::ResolvedTypeReference *targetTypeRef) {
            if (targetTypeRef && targetTypeRef->type.isInlineComponentType()) {
                if (targetTypeRef->type.containingType() == currentICTypeRef->type.containingType()) {
                    auto icIt = std::find_if(allICs.cbegin(), allICs.cend(), [&](const QV4::CompiledData::InlineComponent &icSearched){
                        return int(icSearched.objectIndex) == targetTypeRef->type.inlineComponentObjectId();
                    });
                    Q_ASSERT(icIt != allICs.cend());
                    Node& target = nodes[i];
                    adjacencyList[std::distance(allICs.cbegin(), icIt)].push_back(&target);
                }
            }
        };
        if (obj->inheritedTypeNameIndex != 0) {
            QV4::ResolvedTypeReference *parentTypeRef = objectContainer->resolvedType(obj->inheritedTypeNameIndex);
            createEdgeFromTypeRef(parentTypeRef);

        }
        auto referencedInICObjectIndex = ic.objectIndex + 1;
        while (int(referencedInICObjectIndex) < objectContainer->objectCount()) {
            auto potentiallyReferencedInICObject = objectContainer->objectAt(referencedInICObjectIndex);
            bool stillInIC = !(potentiallyReferencedInICObject-> flags & QV4::CompiledData::Object::IsInlineComponentRoot)
                    && (potentiallyReferencedInICObject-> flags & QV4::CompiledData::Object::InPartOfInlineComponent);
            if (!stillInIC)
                break;
            createEdgeFromTypeRef(objectContainer->resolvedType(potentiallyReferencedInICObject->inheritedTypeNameIndex));
            ++referencedInICObjectIndex;
        }
    }
};

inline void topoVisit(Node *node, AdjacencyList &adjacencyList, bool &hasCycle, std::vector<Node> &nodesSorted) {
    if (node->permanentMark)
        return;
    if (node->temporaryMark) {
        hasCycle = true;
        return;
    }
    node->temporaryMark = 1;

    auto const &edges = adjacencyList[node->index];
    for (auto edgeTarget =edges.begin(); edgeTarget != edges.end(); ++edgeTarget) {
        topoVisit(*edgeTarget, adjacencyList, hasCycle, nodesSorted);
    }

    node->temporaryMark = 0;
    node->permanentMark = 1;
    nodesSorted.push_back(*node);
};

// Use DFS based topological sorting (https://en.wikipedia.org/wiki/Topological_sorting)
inline std::vector<Node> topoSort(std::vector<Node> &nodes, AdjacencyList &adjacencyList, bool &hasCycle) {
    std::vector<Node> nodesSorted;
    nodesSorted.reserve(nodes.size());

    hasCycle = false;
    auto currentNodeIt = std::find_if(nodes.begin(), nodes.end(), [](const Node& node) {
        return node.permanentMark == 0;
    });
    // Do a topological sort of all inline components
    // afterwards, nodesSorted contains the nodes for the inline components in reverse topological order
    while (currentNodeIt != nodes.end() && !hasCycle) {
        Node& currentNode = *currentNodeIt;
        topoVisit(&currentNode, adjacencyList, hasCycle, nodesSorted);
        currentNodeIt = std::find_if(nodes.begin(), nodes.end(), [](const Node& node) {
            return node.permanentMark == 0;
        });
    }
    return nodesSorted;
}
}

#endif // INLINECOMPONENTUTILS_P_H
