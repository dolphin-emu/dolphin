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

#ifndef QOPENGLTEXTURECACHE_P_H
#define QOPENGLTEXTURECACHE_P_H

#include <QtGui/private/qtguiglobal_p.h>
#include <QHash>
#include <QObject>
#include <QCache>
#include <private/qopenglcontext_p.h>
#include <private/qopengltextureuploader_p.h>
#include <QtCore/qmutex.h>

QT_BEGIN_NAMESPACE

class QOpenGLCachedTexture;

class Q_GUI_EXPORT QOpenGLTextureCache : public QOpenGLSharedResource
{
public:
    static QOpenGLTextureCache *cacheForContext(QOpenGLContext *context);

    QOpenGLTextureCache(QOpenGLContext *);
    ~QOpenGLTextureCache();

    GLuint bindTexture(QOpenGLContext *context, const QPixmap &pixmap,
                       QOpenGLTextureUploader::BindOptions options = QOpenGLTextureUploader::PremultipliedAlphaBindOption);
    GLuint bindTexture(QOpenGLContext *context, const QImage &image,
                       QOpenGLTextureUploader::BindOptions options = QOpenGLTextureUploader::PremultipliedAlphaBindOption);

    void invalidate(qint64 key);

    void invalidateResource() override;
    void freeResource(QOpenGLContext *ctx) override;

private:
    GLuint bindTexture(QOpenGLContext *context, qint64 key, const QImage &image, QOpenGLTextureUploader::BindOptions options);

    QMutex m_mutex;
    QCache<quint64, QOpenGLCachedTexture> m_cache;
};

class QOpenGLCachedTexture
{
public:
    QOpenGLCachedTexture(GLuint id, QOpenGLTextureUploader::BindOptions options, QOpenGLContext *context);
    ~QOpenGLCachedTexture() { m_resource->free(); }

    GLuint id() const { return m_resource->id(); }
    QOpenGLTextureUploader::BindOptions options() const { return m_options; }

private:
    QOpenGLSharedResourceGuard *m_resource;
    QOpenGLTextureUploader::BindOptions m_options;
};

QT_END_NAMESPACE

#endif

