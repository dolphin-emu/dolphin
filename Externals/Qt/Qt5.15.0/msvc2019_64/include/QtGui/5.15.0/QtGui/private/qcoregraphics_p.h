/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QCOREGRAPHICS_P_H
#define QCOREGRAPHICS_P_H

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

#include <QtCore/private/qcore_mac_p.h>

#include <QtGui/private/qtguiglobal_p.h>
#include <QtGui/qregion.h>
#include <QtGui/qpalette.h>

#include <CoreGraphics/CoreGraphics.h>

#if defined(__OBJC__) && defined(Q_OS_MACOS)
#include <AppKit/AppKit.h>
#define HAVE_APPKIT
#endif

QT_BEGIN_NAMESPACE

Q_GUI_EXPORT CGBitmapInfo qt_mac_bitmapInfoForImage(const QImage &image);

#ifdef HAVE_APPKIT
Q_GUI_EXPORT QPixmap qt_mac_toQPixmap(const NSImage *image, const QSizeF &size);

QT_END_NAMESPACE
@interface NSImage (QtExtras)
+ (instancetype)imageFromQImage:(const QT_PREPEND_NAMESPACE(QImage) &)image;
+ (instancetype)imageFromQIcon:(const QT_PREPEND_NAMESPACE(QIcon) &)icon;
+ (instancetype)imageFromQIcon:(const QT_PREPEND_NAMESPACE(QIcon) &)icon withSize:(int)size;
@end
QT_BEGIN_NAMESPACE

#endif
Q_GUI_EXPORT CGImageRef qt_mac_toCGImage(const QImage &qImage);
Q_GUI_EXPORT CGImageRef qt_mac_toCGImageMask(const QImage &qImage);
Q_GUI_EXPORT QImage qt_mac_toQImage(CGImageRef image);

Q_GUI_EXPORT void qt_mac_drawCGImage(CGContextRef inContext, const CGRect *inBounds, CGImageRef inImage);

Q_GUI_EXPORT void qt_mac_clip_cg(CGContextRef hd, const QRegion &rgn, CGAffineTransform *orig_xform);

#ifdef HAVE_APPKIT
Q_GUI_EXPORT QColor qt_mac_toQColor(const NSColor *color);
Q_GUI_EXPORT QBrush qt_mac_toQBrush(const NSColor *color, QPalette::ColorGroup colorGroup = QPalette::Normal);
#endif
Q_GUI_EXPORT QColor qt_mac_toQColor(CGColorRef color);
Q_GUI_EXPORT QBrush qt_mac_toQBrush(CGColorRef color);

class Q_GUI_EXPORT QMacCGContext
{
public:
    QMacCGContext() = default;
    QMacCGContext(QPaintDevice *pdev);
    QMacCGContext(QPainter *p);

    operator CGContextRef() { return context; }

private:
    void initialize(QPaintDevice *paintDevice);
    void initialize(const QImage *, QPainter *painter = nullptr);
    QCFType<CGContextRef> context;
};

QT_END_NAMESPACE

#undef HAVE_APPKIT

#endif // QCOREGRAPHICS_P_H
