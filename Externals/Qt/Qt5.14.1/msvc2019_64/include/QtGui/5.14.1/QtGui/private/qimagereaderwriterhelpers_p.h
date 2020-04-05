/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
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

#ifndef QIMAGEREADERWRITERHELPERS_P_H
#define QIMAGEREADERWRITERHELPERS_P_H

#include <QtGui/private/qtguiglobal_p.h>
#include <qsharedpointer.h>
#include "qimageiohandler.h"

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

QT_BEGIN_NAMESPACE

class QFactoryLoader;

namespace QImageReaderWriterHelpers {

enum _qt_BuiltInFormatType {
#ifndef QT_NO_IMAGEFORMAT_PNG
    _qt_PngFormat,
#endif
#ifndef QT_NO_IMAGEFORMAT_BMP
    _qt_BmpFormat,
#endif
#ifndef QT_NO_IMAGEFORMAT_PPM
    _qt_PpmFormat,
    _qt_PgmFormat,
    _qt_PbmFormat,
#endif
#ifndef QT_NO_IMAGEFORMAT_XBM
    _qt_XbmFormat,
#endif
#ifndef QT_NO_IMAGEFORMAT_XPM
    _qt_XpmFormat,
#endif
    _qt_NumFormats,
    _qt_NoFormat = -1
};

#if !defined(QT_NO_IMAGEFORMAT_PPM)
# define MAX_MT_SIZE 20
#elif !defined(QT_NO_IMAGEFORMAT_XBM) || !defined(QT_NO_IMAGEFORMAT_XPM)
#  define MAX_MT_SIZE 10
#else
#  define MAX_MT_SIZE 4
#endif

struct _qt_BuiltInFormatStruct
{
    char extension[4];
    char mimeType[MAX_MT_SIZE];
};

#undef MAX_MT_SIZE

static const _qt_BuiltInFormatStruct _qt_BuiltInFormats[] = {
#ifndef QT_NO_IMAGEFORMAT_PNG
    {"png", "png"},
#endif
#ifndef QT_NO_IMAGEFORMAT_BMP
    {"bmp", "bmp"},
#endif
#ifndef QT_NO_IMAGEFORMAT_PPM
    {"ppm", "x-portable-pixmap"},
    {"pgm", "x-portable-graymap"},
    {"pbm", "x-portable-bitmap"},
#endif
#ifndef QT_NO_IMAGEFORMAT_XBM
    {"xbm", "x-xbitmap"},
#endif
#ifndef QT_NO_IMAGEFORMAT_XPM
    {"xpm", "x-xpixmap"},
#endif
};
Q_STATIC_ASSERT(_qt_NumFormats == sizeof _qt_BuiltInFormats / sizeof *_qt_BuiltInFormats);

#ifndef QT_NO_IMAGEFORMATPLUGIN
QSharedPointer<QFactoryLoader> pluginLoader();
#endif

enum Capability {
    CanRead,
    CanWrite
};
QList<QByteArray> supportedImageFormats(Capability cap);
QList<QByteArray> supportedMimeTypes(Capability cap);
QList<QByteArray> imageFormatsForMimeType(const QByteArray &mimeType, Capability cap);

}

QT_END_NAMESPACE

#endif // QIMAGEREADERWRITERHELPERS_P_H
