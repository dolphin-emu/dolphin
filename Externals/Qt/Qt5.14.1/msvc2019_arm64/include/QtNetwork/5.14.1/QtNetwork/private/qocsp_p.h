/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtNetwork module of the Qt Toolkit.
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

#ifndef QOCSP_P_H
#define QOCSP_P_H

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

// Note, this file is a workaround: on 64-bit Windows one of OpenSSL
// includes combined with openssl/ocsp.h results in macros from
// wincrypt.h exposed. OpenSSL's own very "unique" and "inventive"
// names like OCSP_RESPONSE or X509_NAME were asking to clash with
// other entities (presumably macros) with the same names. Normally,
// ossl_typ.h un-defines them, but due to a bug in OpenSSL, fails
// to do this on Win 64. Thus we have to do it here. We only undef
// 3 names, ossl_typ.h has more, but apparently we don't need them
// (no name clash so far).

QT_REQUIRE_CONFIG(ocsp);

#ifdef Q_OS_WIN
#undef X509_NAME
#undef OCSP_REQUEST
#undef OCSP_RESPONSE
#endif // Q_OS_WIN

#include <openssl/ocsp.h>

#endif // QOCSP_P_H
