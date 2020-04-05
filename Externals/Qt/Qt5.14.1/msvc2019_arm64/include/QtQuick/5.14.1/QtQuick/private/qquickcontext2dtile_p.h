/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#ifndef QQUICKCONTEXT2DTILE_P_H
#define QQUICKCONTEXT2DTILE_P_H

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

#include <private/qtquickglobal_p.h>

QT_REQUIRE_CONFIG(quick_canvas);

#include "qquickcontext2d_p.h"
#if QT_CONFIG(opengl)
# include <QOpenGLFramebufferObject>
#endif
QT_BEGIN_NAMESPACE

class QQuickContext2DTexture;
class QQuickContext2DCommandBuffer;

class QQuickContext2DTile
{
public:
    QQuickContext2DTile();
    virtual ~QQuickContext2DTile();

    bool dirty() const {return m_dirty;}
    void markDirty(bool dirty) {m_dirty = dirty;}

    QRect rect() const {return m_rect;}

    virtual void setRect(const QRect& r) = 0;
    virtual QPainter* createPainter(bool smooth, bool antialiasing);
    virtual void drawFinished() {}

protected:
    virtual void aboutToDraw() {}
    uint m_dirty : 1;
    QRect m_rect;
    QPaintDevice* m_device;
    QPainter m_painter;
};

#if QT_CONFIG(opengl)
class QQuickContext2DFBOTile : public QQuickContext2DTile
{
public:
    QQuickContext2DFBOTile();
    ~QQuickContext2DFBOTile();
    void setRect(const QRect& r) override;
    QOpenGLFramebufferObject* fbo() const {return m_fbo;}
    void drawFinished() override;

protected:
    void aboutToDraw() override;
private:


    QOpenGLFramebufferObject *m_fbo;
};
#endif
class QQuickContext2DImageTile : public QQuickContext2DTile
{
public:
    QQuickContext2DImageTile();
    ~QQuickContext2DImageTile();
    void setRect(const QRect& r) override;
    const QImage& image() const {return m_image;}
private:
    QImage m_image;
};
QT_END_NAMESPACE

#endif // QQUICKCONTEXT2DTILE_P_H
