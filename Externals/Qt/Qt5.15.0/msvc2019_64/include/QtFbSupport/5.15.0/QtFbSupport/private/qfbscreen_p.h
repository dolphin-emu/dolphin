/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QFBSCREEN_P_H
#define QFBSCREEN_P_H

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

#include <qpa/qplatformscreen.h>
#include <QtCore/QTimer>
#include <QtCore/QSize>
#include "qfbcursor_p.h"

QT_BEGIN_NAMESPACE

class QFbWindow;
class QFbCursor;
class QPainter;
class QFbBackingStore;

class QFbScreen : public QObject, public QPlatformScreen
{
    Q_OBJECT

public:
    enum Flag {
        DontForceFirstWindowToFullScreen = 0x01
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    QFbScreen();
    ~QFbScreen();

    virtual bool initialize();

    QRect geometry() const override { return mGeometry; }
    int depth() const override { return mDepth; }
    QImage::Format format() const override { return mFormat; }
    QSizeF physicalSize() const override { return mPhysicalSize; }
    QPlatformCursor *cursor() const override { return mCursor; }

    QWindow *topWindow() const;
    QWindow *topLevelAt(const QPoint & p) const override;

    // compositor api
    virtual void addWindow(QFbWindow *window);
    virtual void removeWindow(QFbWindow *window);
    virtual void raise(QFbWindow *window);
    virtual void lower(QFbWindow *window);
    virtual void topWindowChanged(QWindow *) {}
    virtual int windowCount() const;
    virtual Flags flags() const;

    void addPendingBackingStore(QFbBackingStore *bs) { mPendingBackingStores << bs; }

    void scheduleUpdate();

public slots:
    virtual void setDirty(const QRect &rect);
    void setPhysicalSize(const QSize &size);
    void setGeometry(const QRect &rect);

protected:
    virtual QRegion doRedraw();

    void initializeCompositor();
    bool event(QEvent *event) override;

    QFbWindow *windowForId(WId wid) const;

    QList<QFbWindow *> mWindowStack;
    QRegion mRepaintRegion;
    bool mUpdatePending;

    QFbCursor *mCursor;
    QRect mGeometry;
    int mDepth;
    QImage::Format mFormat;
    QSizeF mPhysicalSize;
    QImage mScreenImage;

private:
    QPainter *mPainter;
    QList<QFbBackingStore*> mPendingBackingStores;

    friend class QFbWindow;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QFbScreen::Flags)

QT_END_NAMESPACE

#endif // QFBSCREEN_P_H
