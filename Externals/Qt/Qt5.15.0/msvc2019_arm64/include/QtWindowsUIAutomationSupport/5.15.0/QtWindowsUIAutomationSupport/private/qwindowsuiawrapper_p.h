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

#ifndef QWINDOWSUIAWRAPPER_H
#define QWINDOWSUIAWRAPPER_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtGui/private/qtguiglobal_p.h>

#include "uiatypes_p.h"
#include "uiaattributeids_p.h"
#include "uiacontroltypeids_p.h"
#include "uiaerrorids_p.h"
#include "uiaeventids_p.h"
#include "uiageneralids_p.h"
#include "uiapatternids_p.h"
#include "uiapropertyids_p.h"
#include "uiaserverinterfaces_p.h"
#include "uiaclientinterfaces_p.h"

QT_REQUIRE_CONFIG(accessibility);

QT_BEGIN_NAMESPACE

class QWindowsUiaWrapper
{
    QWindowsUiaWrapper();
    virtual ~QWindowsUiaWrapper();
public:
    static QWindowsUiaWrapper *instance();
    BOOL ready();
    BOOL clientsAreListening();
    LRESULT returnRawElementProvider(HWND hwnd, WPARAM wParam, LPARAM lParam, IRawElementProviderSimple *el);
    HRESULT hostProviderFromHwnd(HWND hwnd, IRawElementProviderSimple **ppProvider);
    HRESULT raiseAutomationPropertyChangedEvent(IRawElementProviderSimple *pProvider, PROPERTYID id, VARIANT oldValue, VARIANT newValue);
    HRESULT raiseAutomationEvent(IRawElementProviderSimple *pProvider, EVENTID id);
    HRESULT raiseNotificationEvent(IRawElementProviderSimple *provider, NotificationKind notificationKind, NotificationProcessing notificationProcessing, BSTR displayString, BSTR activityId);

private:
    typedef LRESULT (WINAPI *PtrUiaReturnRawElementProvider)(HWND, WPARAM, LPARAM, IRawElementProviderSimple *);
    typedef HRESULT (WINAPI *PtrUiaHostProviderFromHwnd)(HWND, IRawElementProviderSimple **);
    typedef HRESULT (WINAPI *PtrUiaRaiseAutomationPropertyChangedEvent)(IRawElementProviderSimple *, PROPERTYID, VARIANT, VARIANT);
    typedef HRESULT (WINAPI *PtrUiaRaiseAutomationEvent)(IRawElementProviderSimple *, EVENTID);
    typedef HRESULT (WINAPI *PtrUiaRaiseNotificationEvent)(IRawElementProviderSimple *, NotificationKind, NotificationProcessing, BSTR, BSTR);
    typedef BOOL (WINAPI *PtrUiaClientsAreListening)();
    PtrUiaReturnRawElementProvider             m_pUiaReturnRawElementProvider = nullptr;
    PtrUiaHostProviderFromHwnd                 m_pUiaHostProviderFromHwnd = nullptr;
    PtrUiaRaiseAutomationPropertyChangedEvent  m_pUiaRaiseAutomationPropertyChangedEvent = nullptr;
    PtrUiaRaiseAutomationEvent                 m_pUiaRaiseAutomationEvent = nullptr;
    PtrUiaRaiseNotificationEvent               m_pUiaRaiseNotificationEvent = nullptr;
    PtrUiaClientsAreListening                  m_pUiaClientsAreListening = nullptr;
};

QT_END_NAMESPACE

#endif //QWINDOWSUIAWRAPPER_H

