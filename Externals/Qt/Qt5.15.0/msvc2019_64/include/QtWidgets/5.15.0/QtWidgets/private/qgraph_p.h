/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWidgets module of the Qt Toolkit.
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

#ifndef QGRAPH_P_H
#define QGRAPH_P_H

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

#include <QtWidgets/private/qtwidgetsglobal_p.h>
#include <QtCore/QHash>
#include <QtCore/QQueue>
#include <QtCore/QString>
#include <QtCore/QDebug>

#include <functional> // for std::less

#include <float.h>

QT_REQUIRE_CONFIG(graphicsview);

QT_BEGIN_NAMESPACE

template <typename Vertex, typename EdgeData>
class Graph
{
public:
    Graph() {}

    class const_iterator {
    public:
        const_iterator(const Graph *graph, bool begin) : g(graph){
            if (begin) {
                row = g->m_graph.constBegin();
                //test if the graph is empty
                if (row != g->m_graph.constEnd())
                    column = row->cbegin();
            } else {
                row = g->m_graph.constEnd();
            }
        }

        inline Vertex *operator*() {
            return column.key();
        }

        inline Vertex *from() const {
            return row.key();
        }

        inline Vertex *to() const {
            return column.key();
        }

        inline bool operator==(const const_iterator &o) const { return !(*this != o); }
        inline bool operator!=(const const_iterator &o) const {
           if (row ==  g->m_graph.end()) {
                return row != o.row;
           } else {
                return row != o.row || column != o.column;
           }
        }

        // prefix
        const_iterator &operator++() {
            if (row != g->m_graph.constEnd()) {
                ++column;
                if (column == row->cend()) {
                    ++row;
                    if (row != g->m_graph.constEnd()) {
                        column = row->cbegin();
                    }
                }
            }
            return *this;
        }

    private:
        const Graph *g;
        typename QHash<Vertex *, QHash<Vertex *, EdgeData *> >::const_iterator row;
        typename QHash<Vertex *, EdgeData *>::const_iterator column;
    };

    const_iterator constBegin() const {
        return const_iterator(this,true);
    }

    const_iterator constEnd() const {
        return const_iterator(this,false);
    }

    /*!
     * \internal
     *
     * If there is an edge between \a first and \a second, it will return a structure
     * containing the data associated with the edge, otherwise it will return 0.
     *
     */
    EdgeData *edgeData(Vertex* first, Vertex* second) {
        const auto it = m_graph.constFind(first);
        if (it == m_graph.cend())
            return nullptr;
        const auto jt = it->constFind(second);
        if (jt == it->cend())
            return nullptr;
        return *jt;
    }

    void createEdge(Vertex *first, Vertex *second, EdgeData *data)
    {
        // Creates a bidirectional edge
#if defined(QT_DEBUG) && 0
        qDebug("Graph::createEdge(): %s",
               qPrintable(QString::fromLatin1("%1-%2")
                          .arg(first->toString()).arg(second->toString())));
#endif
        if (edgeData(first, second)) {
#ifdef QT_DEBUG
            qWarning("%s-%s already has an edge", qPrintable(first->toString()), qPrintable(second->toString()));
#endif
        }
        createDirectedEdge(first, second, data);
        createDirectedEdge(second, first, data);
    }

    void removeEdge(Vertex *first, Vertex *second)
    {
        // Removes a bidirectional edge
#if defined(QT_DEBUG) && 0
        qDebug("Graph::removeEdge(): %s",
               qPrintable(QString::fromLatin1("%1-%2")
                          .arg(first->toString()).arg(second->toString())));
#endif
        EdgeData *data = edgeData(first, second);
        removeDirectedEdge(first, second);
        removeDirectedEdge(second, first);
        if (data) delete data;
    }

    EdgeData *takeEdge(Vertex* first, Vertex* second)
    {
#if defined(QT_DEBUG) && 0
        qDebug("Graph::takeEdge(): %s",
               qPrintable(QString::fromLatin1("%1-%2")
                          .arg(first->toString()).arg(second->toString())));
#endif
        // Removes a bidirectional edge
        EdgeData *data = edgeData(first, second);
        if (data) {
            removeDirectedEdge(first, second);
            removeDirectedEdge(second, first);
        }
        return data;
    }

    QList<Vertex *> adjacentVertices(Vertex *vertex) const
    {
        const auto it = m_graph.constFind(vertex);
        if (it == m_graph.cend())
            return QList<Vertex *>();
        else
            return it->keys();
    }

    QSet<Vertex*> vertices() const {
        QSet<Vertex *> setOfVertices;
        for (const_iterator it = constBegin(); it != constEnd(); ++it) {
            setOfVertices.insert(*it);
        }
        return setOfVertices;
    }

    QVector<QPair<Vertex*, Vertex*> > connections() const {
        QVector<QPair<Vertex*, Vertex*> > conns;
        for (const_iterator it = constBegin(); it != constEnd(); ++it) {
            Vertex *from = it.from();
            Vertex *to = it.to();
            // do not return (from,to) *and* (to,from)
            if (std::less<Vertex*>()(from, to))
                conns.append(qMakePair(from, to));
        }
        return conns;
    }

#if defined(QT_DEBUG)
    QString serializeToDot() {   // traversal
        QString strVertices;
        QString edges;

        QSet<Vertex *> setOfVertices = vertices();
        for (typename QSet<Vertex*>::const_iterator it = setOfVertices.begin(); it != setOfVertices.end(); ++it) {
            Vertex *v = *it;
            const QList<Vertex*> adjacents = adjacentVertices(v);
            for (auto *v1 : adjacents) {
                EdgeData *data = edgeData(v, v1);
                bool forward = data->from == v;
                if (forward) {
                    edges += QString::fromLatin1("\"%1\"->\"%2\" [label=\"[%3,%4,%5,%6,%7]\" color=\"#000000\"] \n")
                        .arg(v->toString())
                        .arg(v1->toString())
                        .arg(data->minSize)
                        .arg(data->minPrefSize)
                        .arg(data->prefSize)
                        .arg(data->maxPrefSize)
                        .arg(data->maxSize)
                        ;
                }
            }
            strVertices += QString::fromLatin1("\"%1\" [label=\"%2\"]\n").arg(v->toString()).arg(v->toString());
        }
        return QString::fromLatin1("%1\n%2\n").arg(strVertices, edges);
    }
#endif

protected:
    void createDirectedEdge(Vertex *from, Vertex *to, EdgeData *data)
    {
        m_graph[from][to] = data;
    }

    void removeDirectedEdge(Vertex *from, Vertex *to)
    {
        const auto it = m_graph.find(from);
        Q_ASSERT(it != m_graph.end());

        it->remove(to);
        if (it->isEmpty()) {
            //nobody point to 'from' so we can remove it from the graph
            m_graph.erase(it);
        }
    }

private:
    QHash<Vertex *, QHash<Vertex *, EdgeData *> > m_graph;
};

QT_END_NAMESPACE

#endif
