/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#ifndef WINDOWSGUIDSDEFS_P_H
#define WINDOWSGUIDSDEFS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of QtWinExtras. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qglobal.h>
#include <QtCore/qt_windows.h>

QT_BEGIN_NAMESPACE

extern const GUID qCLSID_DestinationList;
extern const GUID qCLSID_EnumerableObjectCollection;
extern const GUID qIID_ICustomDestinationList;
extern const GUID qIID_IApplicationDestinations;
extern const GUID qCLSID_ApplicationDestinations;
extern const GUID qIID_IApplicationDocumentLists;
extern const GUID qCLSID_ApplicationDocumentLists;
extern const GUID qIID_IObjectArray;
extern const GUID qIID_IObjectCollection;
extern const GUID qIID_IPropertyStore;
extern const GUID qIID_ITaskbarList3;
extern const GUID qIID_ITaskbarList4;
extern const GUID qIID_IShellItem2;
extern const GUID qIID_IShellLinkW;
extern const GUID qIID_ITaskbarList;
extern const GUID qIID_ITaskbarList2;
extern const GUID qIID_IUnknown;
extern const GUID qGUID_NULL;

QT_END_NAMESPACE

#endif // WINDOWSGUIDSDEFS_P_H
