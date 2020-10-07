/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtOpenGL module of the Qt Toolkit.
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

#ifndef QGLFRAMEBUFFEROBJECT_P_H
#define QGLFRAMEBUFFEROBJECT_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <qglframebufferobject.h>
#include <private/qglpaintdevice_p.h>
#include <private/qgl_p.h>
#include <private/qopenglextensions_p.h>

QT_BEGIN_NAMESPACE

class QGLFramebufferObjectFormatPrivate
{
public:
    QGLFramebufferObjectFormatPrivate()
        : ref(1),
          samples(0),
          attachment(QGLFramebufferObject::NoAttachment),
          target(GL_TEXTURE_2D),
          mipmap(false)
    {
#ifndef QT_OPENGL_ES_2
        QOpenGLContext *ctx = QOpenGLContext::currentContext();
        const bool isES = ctx ? ctx->isOpenGLES() : QOpenGLContext::openGLModuleType() != QOpenGLContext::LibGL;
        internal_format = isES ? GL_RGBA : GL_RGBA8;
#else
        internal_format = GL_RGBA;
#endif
    }
    QGLFramebufferObjectFormatPrivate
            (const QGLFramebufferObjectFormatPrivate *other)
        : ref(1),
          samples(other->samples),
          attachment(other->attachment),
          target(other->target),
          internal_format(other->internal_format),
          mipmap(other->mipmap)
    {
    }
    bool equals(const QGLFramebufferObjectFormatPrivate *other)
    {
        return samples == other->samples &&
               attachment == other->attachment &&
               target == other->target &&
               internal_format == other->internal_format &&
               mipmap == other->mipmap;
    }

    QAtomicInt ref;
    int samples;
    QGLFramebufferObject::Attachment attachment;
    GLenum target;
    GLenum internal_format;
    uint mipmap : 1;
};

class QGLFBOGLPaintDevice : public QGLPaintDevice
{
public:
    virtual QPaintEngine* paintEngine() const override {return fbo->paintEngine();}
    virtual QSize size() const override {return fbo->size();}
    virtual QGLContext* context() const override;
    virtual QGLFormat format() const override {return fboFormat;}
    virtual bool alphaRequested() const override { return reqAlpha; }

    void setFBO(QGLFramebufferObject* f,
                QGLFramebufferObject::Attachment attachment);

private:
    QGLFramebufferObject* fbo;
    QGLFormat fboFormat;
    bool reqAlpha;
};

class QGLFramebufferObjectPrivate
{
public:
    QGLFramebufferObjectPrivate() : fbo_guard(nullptr), texture_guard(nullptr), depth_buffer_guard(nullptr)
                                  , stencil_buffer_guard(nullptr), color_buffer_guard(nullptr)
                                  , valid(false), engine(nullptr) {}
    ~QGLFramebufferObjectPrivate() {}

    void init(QGLFramebufferObject *q, const QSize& sz,
              QGLFramebufferObject::Attachment attachment,
              GLenum internal_format, GLenum texture_target,
              GLint samples = 0, bool mipmap = false);
    bool checkFramebufferStatus() const;
    QGLSharedResourceGuardBase *fbo_guard;
    QGLSharedResourceGuardBase *texture_guard;
    QGLSharedResourceGuardBase *depth_buffer_guard;
    QGLSharedResourceGuardBase *stencil_buffer_guard;
    QGLSharedResourceGuardBase *color_buffer_guard;
    GLenum target;
    QSize size;
    QGLFramebufferObjectFormat format;
    uint valid : 1;
    QGLFramebufferObject::Attachment fbo_attachment;
    mutable QPaintEngine *engine;
    QGLFBOGLPaintDevice glDevice;
    QOpenGLExtensions funcs;

    inline GLuint fbo() const { return fbo_guard ? fbo_guard->id() : 0; }
};


QT_END_NAMESPACE

#endif // QGLFRAMEBUFFEROBJECT_P_H
