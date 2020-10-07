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

#ifndef QSGENGINE_H
#define QSGENGINE_H

#include <QtCore/QObject>
#include <QtQuick/qtquickglobal.h>

QT_BEGIN_NAMESPACE

class QOpenGLContext;
class QSGAbstractRenderer;
class QSGEnginePrivate;
class QSGTexture;
class QSGRendererInterface;
class QSGRectangleNode;
class QSGImageNode;
class QSGNinePatchNode;

#if QT_DEPRECATED_SINCE(5, 15)
class Q_QUICK_EXPORT QSGEngine : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QSGEngine)
public:
    enum CreateTextureOption {
        TextureHasAlphaChannel  = 0x0001,
        TextureOwnsGLTexture    = 0x0004,
        TextureCanUseAtlas      = 0x0008,
        TextureIsOpaque         = 0x0010
    };
    Q_DECLARE_FLAGS(CreateTextureOptions, CreateTextureOption)
    Q_FLAG(CreateTextureOptions)

    explicit QSGEngine(QObject *parent = nullptr);
    ~QSGEngine() override;

    QT_DEPRECATED_X("QSGEngine is going to be removed in Qt 6.0. Use QQuickRenderControl instead.")
    void initialize(QOpenGLContext *context);
    void invalidate();

    QSGAbstractRenderer *createRenderer() const;
    QSGTexture *createTextureFromImage(const QImage &image, CreateTextureOptions options = CreateTextureOption()) const;
    QSGTexture *createTextureFromId(uint id, const QSize &size, CreateTextureOptions options = CreateTextureOption()) const;
    QSGRendererInterface *rendererInterface() const;
    QSGRectangleNode *createRectangleNode() const;
    QSGImageNode *createImageNode() const;
    QSGNinePatchNode *createNinePatchNode() const;
};
#endif

QT_END_NAMESPACE

#endif // QSGENGINE_H
