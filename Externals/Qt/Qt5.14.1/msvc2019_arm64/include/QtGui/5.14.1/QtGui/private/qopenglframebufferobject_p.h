/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#ifndef QOPENGLFRAMEBUFFEROBJECT_P_H
#define QOPENGLFRAMEBUFFEROBJECT_P_H

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

#include <QtGui/private/qtguiglobal_p.h>
#include <qopenglframebufferobject.h>
#include <private/qopenglcontext_p.h>
#include <private/qopenglextensions_p.h>

QT_BEGIN_NAMESPACE

class QOpenGLFramebufferObjectFormatPrivate
{
public:
    QOpenGLFramebufferObjectFormatPrivate()
        : ref(1),
          samples(0),
          attachment(QOpenGLFramebufferObject::NoAttachment),
          target(GL_TEXTURE_2D),
          mipmap(false)
    {
#ifndef QT_OPENGL_ES_2
        // There is nothing that says QOpenGLFramebufferObjectFormat needs a current
        // context, so we need a fallback just to be safe, even though in pratice there
        // will usually be a context current.
        QOpenGLContext *ctx = QOpenGLContext::currentContext();
        const bool isES = ctx ? ctx->isOpenGLES() : QOpenGLContext::openGLModuleType() != QOpenGLContext::LibGL;
        internal_format = isES ? GL_RGBA : GL_RGBA8;
#else
        internal_format = GL_RGBA;
#endif
    }
    QOpenGLFramebufferObjectFormatPrivate
            (const QOpenGLFramebufferObjectFormatPrivate *other)
        : ref(1),
          samples(other->samples),
          attachment(other->attachment),
          target(other->target),
          internal_format(other->internal_format),
          mipmap(other->mipmap)
    {
    }
    bool equals(const QOpenGLFramebufferObjectFormatPrivate *other)
    {
        return samples == other->samples &&
               attachment == other->attachment &&
               target == other->target &&
               internal_format == other->internal_format &&
               mipmap == other->mipmap;
    }

    QAtomicInt ref;
    int samples;
    QOpenGLFramebufferObject::Attachment attachment;
    GLenum target;
    GLenum internal_format;
    uint mipmap : 1;
};

class QOpenGLFramebufferObjectPrivate
{
public:
    QOpenGLFramebufferObjectPrivate() : fbo_guard(nullptr), depth_buffer_guard(nullptr)
                                  , stencil_buffer_guard(nullptr)
                                  , valid(false) {}
    ~QOpenGLFramebufferObjectPrivate() {}

    void init(QOpenGLFramebufferObject *q, const QSize &size,
              QOpenGLFramebufferObject::Attachment attachment,
              GLenum texture_target, GLenum internal_format,
              GLint samples = 0, bool mipmap = false);
    void initTexture(int idx);
    void initColorBuffer(int idx, GLint *samples);
    void initDepthStencilAttachments(QOpenGLContext *ctx, QOpenGLFramebufferObject::Attachment attachment);

    bool checkFramebufferStatus(QOpenGLContext *ctx) const;
    QOpenGLSharedResourceGuard *fbo_guard;
    QOpenGLSharedResourceGuard *depth_buffer_guard;
    QOpenGLSharedResourceGuard *stencil_buffer_guard;
    GLenum target;
    QSize dsSize;
    QOpenGLFramebufferObjectFormat format;
    int requestedSamples;
    uint valid : 1;
    QOpenGLFramebufferObject::Attachment fbo_attachment;
    QOpenGLExtensions funcs;

    struct ColorAttachment {
        ColorAttachment() : internalFormat(0), guard(nullptr) { }
        ColorAttachment(const QSize &size, GLenum internalFormat)
            : size(size), internalFormat(internalFormat), guard(nullptr) { }
        QSize size;
        GLenum internalFormat;
        QOpenGLSharedResourceGuard *guard;
    };
    QVector<ColorAttachment> colorAttachments;

    inline GLuint fbo() const { return fbo_guard ? fbo_guard->id() : 0; }
};


QT_END_NAMESPACE

#endif // QOPENGLFRAMEBUFFEROBJECT_P_H
