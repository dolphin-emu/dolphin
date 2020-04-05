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

#ifndef QV4_ALLOCA_H
#define QV4_ALLOCA_H

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

#include <QtCore/private/qglobal_p.h>

#if QT_CONFIG(alloca_h)
#  include <alloca.h>
#elif QT_CONFIG(alloca_malloc_h)
#  include <malloc.h>
// This does not matter unless compiling in strict standard mode.
#  ifdef Q_CC_MSVC
#    define alloca _alloca
#  endif
#else
#  include <stdlib.h>
#endif

// Define Q_ALLOCA_VAR macro to be used instead of #ifdeffing
// the occurrences of alloca() in case it's not supported.
// Q_ALLOCA_DECLARE and Q_ALLOCA_ASSIGN macros separate
// memory allocation from the declaration and RAII.
#define Q_ALLOCA_VAR(type, name, size) \
    Q_ALLOCA_DECLARE(type, name); \
    Q_ALLOCA_ASSIGN(type, name, size)

#if QT_CONFIG(alloca)

#define Q_ALLOCA_DECLARE(type, name) \
    type *name = 0

#define Q_ALLOCA_ASSIGN(type, name, size) \
    name = static_cast<type*>(alloca(size))

#else
QT_BEGIN_NAMESPACE
class Qt_AllocaWrapper
{
public:
    Qt_AllocaWrapper() { m_data = 0; }
    ~Qt_AllocaWrapper() { free(m_data); }
    void *data() { return m_data; }
    void allocate(int size) { m_data = malloc(size); memset(m_data, 0, size); }
private:
    void *m_data;
};
QT_END_NAMESPACE

#define Q_ALLOCA_DECLARE(type, name) \
    Qt_AllocaWrapper _qt_alloca_##name; \
    type *name = nullptr

#define Q_ALLOCA_ASSIGN(type, name, size) \
    _qt_alloca_##name.allocate(size); \
    name = static_cast<type*>(_qt_alloca_##name.data())

#endif

#endif
