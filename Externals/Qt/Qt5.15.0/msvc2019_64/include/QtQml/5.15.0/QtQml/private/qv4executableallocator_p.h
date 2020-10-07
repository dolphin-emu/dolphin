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

#ifndef QV4EXECUTABLEALLOCATOR_H
#define QV4EXECUTABLEALLOCATOR_H

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

#include "qv4global_p.h"

#include <QMultiMap>
#include <QHash>
#include <QVector>
#include <QByteArray>
#include <QMutex>

namespace WTF {
class PageAllocation;
}

QT_BEGIN_NAMESPACE

namespace QV4 {

class Q_QML_AUTOTEST_EXPORT ExecutableAllocator
{
public:
    struct ChunkOfPages;
    struct Allocation;

    ExecutableAllocator();
    ~ExecutableAllocator();

    Allocation *allocate(size_t size);
    void free(Allocation *allocation);

    struct Allocation
    {
        Allocation()
            : size(0)
            , free(true)
        {}

        void *exceptionHandler() const;
        void *start() const;
        void invalidate() { addr = 0; }
        bool isValid() const { return addr != 0; }
        void deallocate(ExecutableAllocator *allocator);

    private:
        ~Allocation() {}

        friend class ExecutableAllocator;

        Allocation *split(size_t dividingSize);
        bool mergeNext(ExecutableAllocator *allocator);
        bool mergePrevious(ExecutableAllocator *allocator);

        quintptr addr = 0;
        uint size : 31; // More than 2GB of function code? nah :)
        uint free : 1;
        Allocation *next = nullptr;
        Allocation *prev = nullptr;
    };

    // for debugging / unit-testing
    int freeAllocationCount() const { return freeAllocations.count(); }
    int chunkCount() const { return chunks.count(); }

    struct ChunkOfPages
    {
        ChunkOfPages()

        {}
        ~ChunkOfPages();

        WTF::PageAllocation *pages = nullptr;
        Allocation *firstAllocation = nullptr;

        bool contains(Allocation *alloc) const;
    };

    ChunkOfPages *chunkForAllocation(Allocation *allocation) const;

private:
    QMultiMap<size_t, Allocation*> freeAllocations;
    QMap<quintptr, ChunkOfPages*> chunks;
    mutable QRecursiveMutex mutex;
};

}

QT_END_NAMESPACE

#endif // QV4EXECUTABLEALLOCATOR_H
